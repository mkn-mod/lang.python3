/**
Copyright (c) 2013, Philip Deegan.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
    * Neither the name of Philip Deegan nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "maiken/module/init.hpp"  // IWYU pragma: keep

#include "maiken/app.hpp"       // for Application
#include "maiken/compiler.hpp"  // for CompilationInfo, Mode

#include "mkn/kul/os.hpp"      // for Dir, WHICH, PushDir
#include "mkn/kul/cli.hpp"     // for EnvVar, EnvVarMode
#include "mkn/kul/env.hpp"     // for GET, SET
#include "mkn/kul/log.hpp"     // for KERR
#include "mkn/kul/defs.hpp"    // for MKN_KUL_PUBLISH
#include "mkn/kul/proc.hpp"    // for Process, ProcessCapture, AProcess
#include "mkn/kul/yaml.hpp"    // for NodeValidator, Validator
#include "maiken/project.hpp"  // for Project
#include "mkn/kul/except.hpp"  // for Exception, KEXCEPT, KTHROW
#include "mkn/kul/string.hpp"  // for String

#include <memory>     // for shared_ptr, make_shared
#include <string>     // for basic_string, string
#include <vector>     // for vector
#include <stdlib.h>   // for exit
#include <exception>  // for exception

namespace mkn {
namespace lang {

class Python3Module : public maiken::Module {
 public:
  void init(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override {
    if (!kul::env::WHICH(PY.c_str())) PY = "python";
    PYTHON = kul::env::GET("PYTHON");
    if (!PYTHON.empty()) PY = PYTHON;
#if defined(_WIN32)
    if (PY.rfind(".exe") == std::string::npos) PY += ".exe";
#endif
    kul::Process p(PY);
    kul::ProcessCapture pc(p);
    HOME = kul::env::GET("PYTHON3_HOME");
    if (!HOME.empty()) {
#if defined(_WIN32)
      bin = kul::Dir(HOME);
      if (!bin) KEXCEPT(kul::Exception, "$PYTHON3_HOME does not exist");
#else
      bin = kul::Dir("bin", HOME);
      if (!bin) KEXCEPT(kul::Exception, "$PYTHON3_HOME/bin does not exist");
#endif
      path_var = std::make_shared<kul::cli::EnvVar>("PATH", bin.real(), kul::cli::EnvVarMode::PREP);
      kul::env::SET(path_var->name(), path_var->toString().c_str());
      p.var(path_var->name(), path_var->toString());
    };
  }

  void compile(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override {
    VALIDATE_NODE(node);

    auto const include = py_include();
    KLOG(INF) << include;
    std::vector<std::string> incs;
    incs.push_back(include);

    try {
      if (node["with"]) {
        for (auto const with : kul::cli::asArgs(node["with"].Scalar())) {
          std::stringstream import;
          import << "\"import " << with << "; print(" << with << ".get_include())\"";
          auto outs = pyexec_for_string(import.str());
          outs.pop_back();
#if defined(_WIN32)
          outs.pop_back();
#endif
          incs.push_back(outs);
        }
      }
      for (auto const inc : incs) {
        kul::Dir req_include(inc);
        if (req_include) {
          a.addInclude(req_include.real());
          for (auto* rep : a.revendencies()) rep->addInclude(req_include.real());
        }
      }
    } catch (kul::Exception const& e) {
      KERR << e.stack();
    } catch (std::exception const& e) {
      KERR << e.what();
    } catch (...) {
      KERR << "UNKNOWN ERROR CAUGHT";
    }
  }

  //   void compile(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override
  //   {
  //     VALIDATE_NODE(node);
  //     kul::os::PushDir pushd(a.project().dir());
  //     std::vector<std::string> incs;
  //     try {
  //       if (pyconfig_found) {
  //         kul::Process p(PY3_CONFIG);
  //         kul::ProcessCapture pc(p);
  //         p << "--includes";
  //         if (path_var) p.var(path_var->name(), path_var->toString());
  //         p.start();
  //         auto outs(pc.outs());
  //         outs.pop_back();
  //         for (auto inc : kul::cli::asArgs(outs)) {
  //           if (inc.find("-I") == 0) inc = inc.substr(2);
  //           incs.push_back(inc);
  //         }
  //       } else {
  //         kul::Dir dInc;
  //         if (path_var) {
  //           dInc = kul::Dir("include", bin.parent());
  //           if (!dInc) dInc = kul::Dir("include", kul::File(kul::env::WHERE(PY.c_str())).dir());
  //         } else {
  //           dInc = kul::Dir("include", kul::File(kul::env::WHERE(PY.c_str())).dir());
  //         }
  //         if (!dInc)
  //           KEXCEPT(kul::Exception, "$PYTHON3_HOME/include does not exist")
  //               << kul::os::EOL() << dInc.path();
  //         incs.push_back(dInc.real());
  //       }
  //     } catch (kul::Exception const& e) {
  //       KERR << e.stack();
  //     } catch (std::exception const& e) {
  //       KERR << e.what();
  //     } catch (...) {
  //       KERR << "UNKNOWN ERROR CAUGHT";
  //     }

  //     try {
  //       if (node["with"]) {
  //         for (auto const with : kul::cli::asArgs(node["with"].Scalar())) {
  //           std::stringstream import;
  //           import << "\"import " << with << "; print(" << with << ".get_include())\"";
  //           kul::Process p(PY);
  //           kul::ProcessCapture pc(p);
  //           p << "-c" << import.str();
  //           if (path_var) p.var(path_var->name(), path_var->toString());
  //           p.start();
  //           auto outs(pc.outs());
  //           outs.pop_back();
  // #if defined(_WIN32)
  //           outs.pop_back();
  // #endif
  //           incs.push_back(outs);
  //         }
  //       }
  //       for (auto const inc : incs) {
  //         kul::Dir req_include(inc);
  //         if (req_include) {
  //           a.addInclude(req_include.real());
  //           for (auto* rep : a.revendencies()) rep->addInclude(req_include.real());
  //         }
  //       }
  //     } catch (kul::Exception const& e) {
  //       KERR << e.stack();
  //     } catch (std::exception const& e) {
  //       KERR << e.what();
  //     } catch (...) {
  //       KERR << "UNKNOWN ERROR CAUGHT";
  //     }
  //   }

  void link(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override {
    VALIDATE_NODE(node);

    auto const embed = kul::String::BOOL(kul::env::GET("MKN_PYTHON_LIB_EMBED", "0"));

    auto linker = py_cflags();
    auto const libpath = py_libdir();
    std::cout << __FILE__ << " " << __LINE__ << " " << libpath << std::endl;
    auto const prefx = py_prefix();
    KLOG(INF) << prefx;
    if (prefx.size())
      if (auto const lib = kul::Dir(kul::Dir::JOIN(prefx, "lib"))) {
        if (auto const needle = std::string{"-L" + lib.real()};
            linker.find(needle) != std::string::npos) {
          kul::String::REPLACE_ALL(linker, needle + " ", "");
          a.addLibpath(lib.real());
        }
      }

    if (embed) {
      auto const lib = py_libname();
      KLOG(INF) << lib;
      a.addLib(lib);
    }

    if (a.mode() != maiken::compiler::Mode::STAT) a.prependLinkString(linker);
  }

  //   void link(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override {
  //     VALIDATE_NODE(node);
  //     if (pyconfig_found) {
  //       auto version = MajMin(PY);

  //       kul::os::PushDir pushd(a.project().dir());
  //       kul::Process p(PY3_CONFIG);
  //       kul::ProcessCapture pc(p);
  //       p << "--ldflags";

  //       if (path_var) p.var(path_var->name(), path_var->toString());

  //       auto const embed = kul::String::BOOL(kul::env::GET("MKN_PYTHON_LIB_EMBED", "0"));
  //       if (embed) p << "--embed";

  //       p.start();
  //       std::string linker(pc.outs());
  //       linker.pop_back();
  //       auto const prefx = prefix();

  //       if (prefx.size())
  //         if (auto const lib = kul::Dir(kul::Dir::JOIN(prefx, "lib"))) {
  //           if (auto const needle = std::string{"-L" + lib.real()};
  //               linker.find(needle) != std::string::npos) {
  //             kul::String::REPLACE_ALL(linker, needle + " ", "");
  //             a.addLibpath(lib.real());
  //           }
  //         }

  //       if (embed)
  //         for (auto bit : kul::String::SPLIT(linker, " ")) {
  //           kul::String::REPLACE_ALL(bit, " ", "");
  //           if (bit.find("-l") == 0) {
  //             auto const lib = bit.substr(2);
  //             kul::String::REPLACE_ALL(linker, bit + " ", "");
  //             a.addLib(lib);
  //           }
  //         }

  //       if (node["delete"]) {
  //         kul::String::REPLACE_ALL(linker, "  ", " ");
  //         for (auto const del : kul::String::SPLIT(node["delete"].Scalar(), " "))
  //           kul::String::REPLACE_ALL(linker, del, "");
  //         kul::String::REPLACE_ALL(linker, "  ", " ");
  //       }
  //       if (a.mode() != maiken::compiler::Mode::STAT) a.prependLinkString(linker);
  //     } else {
  //       kul::Dir dPath;
  //       if (path_var) {
  // #if defined(_WIN32)
  //         dPath = kul::Dir("libs", bin);
  // #endif
  //       } else {
  //         dPath = kul::Dir("libs", kul::File(kul::env::WHERE(PY.c_str())).dir());
  //       }
  //       if (!dPath) KEXCEPT(kul::Exception, "$PYTHON3_HOME/libs does not exist");
  //       a.addLibpath(dPath.real());
  //     }
  //   }

 private:
  std::string pyexec_for_string(std::string const& cmd) const {
    mkn::kul::Process p(PY);
    mkn::kul::ProcessCapture pc(p);
    p << "-c" << cmd;

    KLOG(INF) << p.toString();
    // try {
    p.start();
    KLOG(INF) << "FINE " << pc.outs();
    return pc.outs();
    // } catch (mkn::kul::proc::ExitException const& e) {
    //   KLOG(INF) << pc.outs();
    //   KLOG(INF) << pc.errs();
    //   KLOG(INF) << e.what();
    // }

    return "FAIL";
  }
  std::string py_include() const {
    // return pyexec_for_string("\"import sysconfig;
    // print('-I'+sysconfig.get_paths()['include'])\""); import sysconfig;
    // print('-I'+sysconfig.get_paths()['include'])
    return pyexec_for_string("\"import sysconfig; print('-I'+sysconfig.get_paths()['include']) \"");
  }

  std::string py_cflags() const {
    return pyexec_for_string(
        "\"import sysconfig; print(sysconfig.get_config_var(\'CFLAGS\') or \'\')\"");
  }

  std::string py_libdir() const {
    return pyexec_for_string("\"import sysconfig; print(sysconfig.get_config_var('LIBDIR'))\"");
  }

  std::string py_libname() const {
    return pyexec_for_string(
        "\"import sysconfig; lib=sysconfig.get_config_var('LDLIBRARY');"
        "print(lib[3:-3] if lib.startswith('lib') and lib.endswith('.so') else lib)\"");
  }
  std::string py_prefix() const {
    return pyexec_for_string("\"import sysconfig; print(sysconfig.get_config_var('prefix'))\"");
  }

  static std::vector<uint16_t> MajMin(std::string const& PY) {
    std::vector<uint16_t> version(2);

    for (auto const& idx : {0, 1}) {
      kul::Process p(PY);
      kul::ProcessCapture pc(p);
      std::string print{"\"import sys; print(sys.version_info[" + std::to_string(idx) + "])\""};
      p << "-c" << print;
      p.start();

      auto out = kul::String::LINES(pc.outs())[0];
      kul::String::TRIM(out);

      version[idx] = kul::String::UINT16(out);
    }

    return version;
  }

  static void VALIDATE_NODE(YAML::Node const& node) {
    using namespace kul::yaml;
    Validator({
                  NodeValidator("args"),
                  NodeValidator("delete"),
                  NodeValidator("with"),
                  NodeValidator("min"),
              })
        .validate(node);
  }

  std::string HOME;
  std::string PY = "python3";
  std::string PYTHON;
  std::string PATH = kul::env::GET("PATH");

  kul::Dir bin;
  std::shared_ptr<kul::cli::EnvVar> path_var;
};
}  // namespace lang
}  // namespace mkn

extern "C" MKN_KUL_PUBLISH maiken::Module* maiken_module_construct() {
  return new mkn::lang::Python3Module;
}

extern "C" MKN_KUL_PUBLISH void maiken_module_destruct(maiken::Module* p) { delete p; }
