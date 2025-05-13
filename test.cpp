
#include <maiken.hpp>

#include "mkn/kul/signal.hpp"
#include "mkn/kul/yaml.hpp"

std::string const yArgs = "";

int main(int argc, char* argv[]) {
  mkn::kul::Signal sig;
  try {
    YAML::Node node = mkn::kul::yaml::String(yArgs).root();
    char* argv2[2] = {argv[0], (char*)"-O"};
    char* argv3[3] = {argv[0], (char*)"-Op", (char*)"test"};
    auto app = (maiken::Application::CREATE(2, argv2))[0];
    auto loader(maiken::ModuleLoader::LOAD(*app));
    auto appTest = (maiken::Application::CREATE(3, argv3))[0];
    loader->module()->init(*appTest, node);
    loader->module()->compile(*appTest, node);
    loader->module()->link(*appTest, node);
    loader->module()->pack(*appTest, node);
    loader->unload();
  } catch (mkn::kul::Exception const& e) {
    KLOG(ERR) << e.what();
    return 2;
  } catch (std::exception const& e) {
    KERR << e.what();
    return 3;
  } catch (...) {
    KERR << "UNKNOWN EXCEPTION TYPE CAUGHT";
    return 5;
  }
  return 0;
}
