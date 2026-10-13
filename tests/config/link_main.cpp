#include "link_shared.hpp"

int main() {
  lock_align::ordered_mutex<std::mutex, 1> m;
  use_mutex(m);
}
