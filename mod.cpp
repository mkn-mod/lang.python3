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

#include "maiken.hpp"
#define KUL_EXPORT
#undef _KUL_DEFS_HPP_
#include "kul/defs.hpp"

#include <unordered_set>

namespace mkn {
namespace lang {

class Python3Module : public maiken::Module {
 private:
  std::string HOME, PY = "python3", PY_CONFIG = "python3-config",
                    PATH = kul::env::GET("PATH");
  ;
  kul::Dir bin;
  std::shared_ptr<kul::cli::EnvVar> path_var;

 protected:
  static void VALIDATE_NODE(const YAML::Node& node) {
    using namespace kul::yaml;
    Validator({
                  NodeValidator("args"),
                  NodeValidator("delete"),
                  NodeValidator("with"),
                  NodeValidator("min"),
              })
        .validate(node);
  }

 public:
  void init(maiken::Application& a, const YAML::Node& node)
      KTHROW(std::exception) override {
    kul::Process p(PY);
    kul::ProcessCapture pc(p);
    HOME = kul::env::GET("PYTHON3_HOME");
    if (!HOME.empty()) {
      bin = kul::Dir("bin", HOME);
      if (!bin) KEXCEPT(kul::Exception, "$PYTHON3_HOME/bin does not exist");
      path_var = std::make_shared<kul::cli::EnvVar>("PATH", bin.real(),
                                                    kul::cli::EnvVarMode::PREP);
      p.var(path_var->name(), path_var->toString());
    };
    try {
      p << "-c"
        << "\"import sys; print(sys.version_info[0])\"";
      p.start();
      KLOG(INF) << pc.outs();
    } catch (const kul::Exception& e) {
      KERR << e.stack();
    } catch (const std::exception& e) {
      KERR << e.what();
    } catch (...) {
      KERR << "UNKNOWN ERROR CAUGHT";
    }
  }

  void compile(maiken::Application& a, const YAML::Node& node)
      KTHROW(std::exception) {
    VALIDATE_NODE(node);
    kul::os::PushDir pushd(a.project().dir());
    {
      kul::Process p(PY_CONFIG);
      kul::ProcessCapture pc(p);
      p << "--includes";
      if (path_var) p.var(path_var->name(), path_var->toString());
      p.start();
      KLOG(INF) << pc.outs();
    }
    if (node["with"]) {
      for (const auto with : kul::cli::asArgs(node["with"].Scalar())) {
        std::stringstream import;
        import << "\"import " << with << "; print(" << with
               << ".get_include())\"";
        kul::Process p(PY);
        kul::ProcessCapture pc(p);
        p << "-c" << import.str();
        if (path_var) p.var(path_var->name(), path_var->toString());
        p.start();
        KLOG(INF) << pc.outs();
      }
    }
    // a.prepend_compiler_arguments();
  }

  void link(maiken::Application& a, const YAML::Node& node)
      KTHROW(std::exception) override {
    VALIDATE_NODE(node);
    kul::os::PushDir pushd(a.project().dir());
    kul::Process p(PY_CONFIG);
    kul::ProcessCapture pc(p);
    p << "--ldflags";
    if (path_var) p.var(path_var->name(), path_var->toString());
    p.start();
    KLOG(INF) << pc.outs();
    std::string linker(pc.outs());
    if (node["delete"]) {
      for (const auto with : kul::cli::asArgs(node["delete"].Scalar())) {
        kul::String::REPLACE_ALL(linker, with, "");
      }
      kul::String::REPLACE_ALL(linker, "  ", " ");
      kul::String::REPLACE_ALL(linker, "  ", " ");
    }
    KLOG(INF) << linker;
    // a.prepend_linker_arguments();
  }
};
}  // namespace lang
}  // namespace mkn

extern "C" KUL_PUBLISH maiken::Module* maiken_module_construct() {
  return new mkn ::lang ::Python3Module;
}

extern "C" KUL_PUBLISH void maiken_module_destruct(maiken::Module* p) {
  delete p;
}
