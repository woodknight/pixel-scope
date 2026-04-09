#include <optional>
#include <string>

#include "ui/app.h"

int main(int argc, char** argv) {
  std::optional<std::string> initial_path;
  if (argc > 1) {
    initial_path = argv[1];
  }

  pixelscope::ui::App app(initial_path);
  if (!app.initialize()) {
    return 1;
  }
  return app.run();
}
