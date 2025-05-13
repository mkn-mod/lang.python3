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
#include "maiken/module/init.hpp"
#include "mkn/kul/string.hpp"

#include <memory>
#include <vector>

namespace mkn {
namespace lang {

class Python3Module : public maiken::Module {
 private:
#if defined(_WIN32)
  bool const config_expected = 0;
#else
  bool const config_expected = 1;
#endif
  bool pyconfig_found = 0;
  std::string HOME, PY = "python3", PYTHON, PY_CONFIG = "python-config",
                    PY3_CONFIG = "python3-config", PATH = mkn::kul::env::GET("PATH");
  mkn::kul::Dir bin;
  std::shared_ptr<kul::cli::EnvVar> path_var;

 protected:
  static std::vector<uint16_t> MajMin(std::string const& PY) {
    std::vector<uint16_t> version(2);

    for (auto const& idx : {0, 1}) {
      mkn::kul::Process p(PY);
      mkn::kul::ProcessCapture pc(p);
      std::string print{"\"import sys; print(sys.version_info[" + std::to_string(idx) + "])\""};
      p << "-c" << print;
      p.start();

      auto out = mkn::kul::String::LINES(pc.outs())[0];
      mkn::kul::String::TRIM(out);

      version[idx] = mkn::kul::String::UINT16(out);
    }

    return version;
  }

  static void VALIDATE_NODE(YAML::Node const& node) {
    using namespace mkn::kul::yaml;
    Validator({
                  NodeValidator("args"),
                  NodeValidator("delete"),
                  NodeValidator("with"),
                  NodeValidator("min"),
              })
        .validate(node);
  }

 public:
  void init(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override {
    bool finally = 0;
    if (!kul::env::WHICH(PY.c_str())) PY = "python";
    PYTHON = mkn::kul::env::GET("PYTHON");
    if (!PYTHON.empty()) PY = PYTHON;
#if defined(_WIN32)
    if (PY.rfind(".exe") == std::string::npos) PY += ".exe";
#endif
    mkn::kul::Process p(PY);
    mkn::kul::ProcessCapture pc(p);
    HOME = mkn::kul::env::GET("PYTHON3_HOME");
    if (!HOME.empty()) {
#if defined(_WIN32)
      bin = mkn::kul::Dir(HOME);
      if (!bin) KEXCEPT(kul::Exception, "$PYTHON3_HOME does not exist");
#else
      bin = mkn::kul::Dir("bin", HOME);
      if (!bin) KEXCEPT(kul::Exception, "$PYTHON3_HOME/bin does not exist");
#endif
      path_var =
          std::make_shared<kul::cli::EnvVar>("PATH", bin.real(), mkn::kul::cli::EnvVarMode::PREP);
      mkn::kul::env::SET(path_var->name(), path_var->toString().c_str());
      p.var(path_var->name(), path_var->toString());
    };
    pyconfig_found = mkn::kul::env::WHICH(PY3_CONFIG.c_str());
    if (!pyconfig_found) {
      pyconfig_found = mkn::kul::env::WHICH(PY_CONFIG.c_str());
      PY3_CONFIG = PY_CONFIG;
    }
    try {
      if (!pyconfig_found && config_expected) {
        finally = 1;
        KEXCEPT(kul::Exception, "python-config does not exist on path");
      }
      p << "-c"
        << "\"import sys; print(sys.version_info[0])\"";
      p.start();
    } catch (mkn::kul::Exception const& e) {
      KERR << e.stack();
    } catch (std::exception const& e) {
      KERR << e.what();
    } catch (...) {
      KERR << "UNKNOWN ERROR CAUGHT";
    }
    if (finally) exit(2);
  }

  void compile(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override {
    VALIDATE_NODE(node);
    mkn::kul::os::PushDir pushd(a.project().dir());
    std::vector<std::string> incs;
    try {
      if (pyconfig_found) {
        mkn::kul::Process p(PY3_CONFIG);
        mkn::kul::ProcessCapture pc(p);
        p << "--includes";
        if (path_var) p.var(path_var->name(), path_var->toString());
        p.start();
        auto outs(pc.outs());
        outs.pop_back();
        for (auto inc : mkn::kul::cli::asArgs(outs)) {
          if (inc.find("-I") == 0) inc = inc.substr(2);
          incs.push_back(inc);
        }
      } else {
        mkn::kul::Dir dInc;
        if (path_var) {
          dInc = mkn::kul::Dir("include", bin.parent());
          if (!dInc)
            dInc = mkn::kul::Dir("include", mkn::kul::File(kul::env::WHERE(PY.c_str())).dir());
        } else {
          dInc = mkn::kul::Dir("include", mkn::kul::File(kul::env::WHERE(PY.c_str())).dir());
        }
        if (!dInc)
          KEXCEPT(kul::Exception, "$PYTHON3_HOME/include does not exist")
              << mkn::kul::os::EOL() << dInc.path();
        incs.push_back(dInc.real());
      }
    } catch (mkn::kul::Exception const& e) {
      KERR << e.stack();
    } catch (std::exception const& e) {
      KERR << e.what();
    } catch (...) {
      KERR << "UNKNOWN ERROR CAUGHT";
    }

    try {
      if (node["with"]) {
        for (auto const with : mkn::kul::cli::asArgs(node["with"].Scalar())) {
          std::stringstream import;
          import << "\"import " << with << "; print(" << with << ".get_include())\"";
          mkn::kul::Process p(PY);
          mkn::kul::ProcessCapture pc(p);
          p << "-c" << import.str();
          if (path_var) p.var(path_var->name(), path_var->toString());
          p.start();
          auto outs(pc.outs());
          outs.pop_back();
#if defined(_WIN32)
          outs.pop_back();
#endif
          incs.push_back(outs);
        }
      }
      for (auto const inc : incs) {
        mkn::kul::Dir req_include(inc);
        if (req_include) {
          a.addInclude(req_include.real());
          for (auto* rep : a.revendencies()) rep->addInclude(req_include.real());
        }
      }
    } catch (mkn::kul::Exception const& e) {
      KERR << e.stack();
    } catch (std::exception const& e) {
      KERR << e.what();
    } catch (...) {
      KERR << "UNKNOWN ERROR CAUGHT";
    }
  }

  void link(maiken::Application& a, YAML::Node const& node) KTHROW(std::exception) override {
    VALIDATE_NODE(node);
    if (pyconfig_found) {
      auto version = MajMin(PY);

      mkn::kul::os::PushDir pushd(a.project().dir());
      mkn::kul::Process p(PY3_CONFIG);
      mkn::kul::ProcessCapture pc(p);
      p << "--ldflags";

      if (path_var) p.var(path_var->name(), path_var->toString());

      auto embed = PYTHON = mkn::kul::env::GET("MKN_PYTHON_LIB_EMBED", "0");
      if (mkn::kul::String::BOOL(embed)) p << "--embed";

      p.start();
      std::string linker(pc.outs());
      linker.pop_back();
      if (node["delete"]) {
        mkn::kul::String::REPLACE_ALL(linker, "  ", " ");
        for (auto const del : mkn::kul::String::SPLIT(node["delete"].Scalar(), " "))
          mkn::kul::String::REPLACE_ALL(linker, del, "");
        mkn::kul::String::REPLACE_ALL(linker, "  ", " ");
      }
      if (a.mode() != maiken::compiler::Mode::STAT) a.prependLinkString(linker);
    } else {
      mkn::kul::Dir dPath;
      if (path_var) {
#if defined(_WIN32)
        dPath = mkn::kul::Dir("libs", bin);
#endif
      } else {
        dPath = mkn::kul::Dir("libs", mkn::kul::File(kul::env::WHERE(PY.c_str())).dir());
      }
      if (!dPath) KEXCEPT(kul::Exception, "$PYTHON3_HOME/libs does not exist");
      a.addLibpath(dPath.real());
    }
  }
};
}  // namespace lang
}  // namespace mkn

extern "C" KUL_PUBLISH maiken::Module* maiken_module_construct() {
  return new mkn::lang::Python3Module;
}

extern "C" KUL_PUBLISH void maiken_module_destruct(maiken::Module* p) { delete p; }
