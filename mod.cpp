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

#include <string>     // for basic_string, string
#include <vector>     // for vector
#include <sstream>    // for stringstream
#include <stdlib.h>   // for exit
#include <exception>  // for exception
#include <stdexcept>  // for exception

namespace mkn {
namespace lang {

kul::File find_python3() KTHROW(std::exception) {
  std::string const HOME = kul::env::GET("PYTHON3_HOME");

  kul::Dir dir;
  if (!HOME.empty()) {
#if defined(_WIN32)
    dir = kul::Dir(HOME);
    if (!dir) KEXCEPT(kul::Exception, "$PYTHON3_HOME does not exist");
#else
    dir = kul::Dir("bin", HOME);
    if (!dir) KEXCEPT(kul::Exception, "$PYTHON3_HOME/bin does not exist");
#endif
  }

  std::vector<std::string> bins{"python3", "python"};
  for (auto const& bin : bins)
    if (kul::env::WHICH(bin.c_str()))
      return dir ? kul::File{bin, dir} : kul::env::WHERE(bin.c_str());

#if defined(_WIN32)  // or fallback
  std::vector<std::string> exes{"python3.exe", "python.exe"};
  for (auto const& bin : exes)
    if (kul::env::WHICH(bin.c_str()))
      return dir ? kul::File{bin, dir} : kul::env::WHERE(bin.c_str());
#endif

  throw std::runtime_error("Could not find python!");
}

kul::cli::EnvVar python3_path_var(kul::File const& exe) {
  return {"PATH", exe.dir().real(), kul::cli::EnvVarMode::PREP};
}

kul::File find_python_set_env() {
  auto const python_exe = find_python3();
  // if (!python_exe) throw std::runtime_error("Could not find python!");
  // auto const path_var = python3_path_var(python_exe);
  // kul::env::SET(path_var.name(), path_var.toString().c_str());
  return python_exe;
}

static inline kul::File const python_exe = find_python_set_env();

std::string pyexec_for_string(std::string const& cmd) {
  auto const path_var = python3_path_var(python_exe);
  kul::Process p(python_exe.real());
  kul::ProcessCapture pc(p);
  p << "-c" << ("\"" + cmd + "\"");
  p.var(path_var.name(), path_var.toString());

  try {
    p.start();
  } catch (kul::proc::ExitException const& ex) {
    KLOG(ERR) << pc.outs();
    KLOG(ERR) << pc.errs();
    KERR << ex;
    throw;
  }
  auto ret = kul::String::LINES(pc.outs())[0];
  if(ret.back() == '\n') ret.pop_back();
  if(ret.back() == '\r') ret.pop_back();
  return ret;
}

class Python3Module : public maiken::Module {
 public:
  void compile(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override;
  void link(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override;

 private:
  auto py_include() const {
    return pyexec_for_string("import sysconfig; print(sysconfig.get_paths()['include'])");
  }

  std::string py_cflags() const {
    return pyexec_for_string("import sysconfig; print(sysconfig.get_config_var('CFLAGS') or '')");
  }

  std::string py_libdir() const {
    return pyexec_for_string("import sysconfig; print(sysconfig.get_config_var('LIBDIR'))");
  }

  std::string py_libname() const {
    return pyexec_for_string(
        "import sysconfig; lib=sysconfig.get_config_var('LDLIBRARY');"
        "print(lib[3:].split('.')[0] if lib.startswith('lib') else lib.split('.')[0])");
  }
  std::string py_prefix() const {
    return pyexec_for_string("import sysconfig; print(sysconfig.get_config_var('prefix'))");
  }

  static std::vector<uint16_t> MajMin(std::string const& PY) {
    std::vector<uint16_t> version(2);

    for (auto const& idx : {0, 1}) {
      kul::Process p(PY);
      kul::ProcessCapture pc(p);
      std::string print{"import sys; print(sys.version_info[" + std::to_string(idx) + "])"};
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
};

void Python3Module::compile(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) {
  VALIDATE_NODE(node);

  std::vector<std::string> incs{py_include()};
#if defined(_WIN32)  // or fallback
  incs.push_back(kul::Dir{"include", python_exe.dir()}.escm());
#endif

  try {
    if (node["with"]) {
      for (auto const& with : kul::cli::asArgs(node["with"].Scalar())) {
        // std::stringstream import;
        auto const import = std::string{"import "} + with + "; print(" + with + ".get_include())";
        // auto outs = pyexec_for_string(import);
        //         outs.pop_back();
        // #if defined(_WIN32)
        //         outs.pop_back();
        // #endif
        incs.push_back(pyexec_for_string(import));
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

void Python3Module::link(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) {
  VALIDATE_NODE(node);

  auto const embed = kul::String::BOOL(kul::env::GET("MKN_PYTHON_LIB_EMBED", "0"));
  auto linker = py_cflags();
  auto const libpath = py_libdir();
  auto const prefx = py_prefix();

  if (prefx.size())
    if (auto const lib = kul::Dir(kul::Dir::JOIN(prefx, "lib"))) {
      if (auto const needle = std::string{"-L" + lib.real()};
          linker.find(needle) != std::string::npos) {
        kul::String::REPLACE_ALL(linker, needle + " ", "");
        a.addLibpath(lib.real());
      }
    }

#if defined(_WIN32)  // or fallback
  if (prefx.size())
    if (auto const lib = kul::Dir{"libs", python_exe.dir()})  // windows fallback
      a.addLibpath(lib.escm());
#endif

  if (embed) a.addLib(py_libname());

  if (a.mode() != maiken::compiler::Mode::STAT) a.prependLinkString(linker);
}

}  // namespace lang
}  // namespace mkn

extern "C" MKN_KUL_PUBLISH maiken::Module* maiken_module_construct() {
  return new mkn::lang::Python3Module;
}

extern "C" MKN_KUL_PUBLISH void maiken_module_destruct(maiken::Module* p) { delete p; }
