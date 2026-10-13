// Two layers of locks: the catalog, then the items in it. Locking them in the
// wrong order is reported the first time that code runs, even though this
// program has only one thread and could never deadlock by itself.

#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <utility>

#include "lock_align/ordered_mutex.hpp"
#include "lock_align/violation.hpp"

using namespace lock_align;

// Lower levels are locked first.
enum class level { catalog = 1, item = 2 };

struct item {
  explicit item(std::string n) : name(std::move(n)), mutex(name) {}

  std::string name;
  int stock = 0;
  ordered_mutex<std::mutex, level::item> mutex;
};

class catalog {
 public:
  item& add(const std::string& name) {
    std::lock_guard g(mutex_);
    return items_.try_emplace(name, name).first->second;
  }

  // Right order: the catalog, then the item.
  void restock(const std::string& name, int count) {
    std::lock_guard g(mutex_);
    item& it = items_.at(name);
    std::lock_guard gi(it.mutex);
    it.stock += count;
  }

  // Wrong order: the item, then the catalog. A thread in here and another in
  // restock() for the same item could each wait for the other forever.
  void sell(item& it) {
    std::lock_guard gi(it.mutex);
    --it.stock;
    std::lock_guard g(mutex_);  // reported here, before it can block
    ++sales_;
  }

  int sales() {
    std::lock_guard g(mutex_);
    return sales_;
  }

 private:
  ordered_mutex<std::mutex, level::catalog> mutex_{"catalog"};
  std::map<std::string, item> items_;
  int sales_ = 0;
};

int main() {
  catalog shop;
  item& widget = shop.add("widget");
  shop.restock("widget", 5);
  std::printf("restocked: %d widgets\n", widget.stock);

  try {
    shop.sell(widget);
  } catch (const lock_order_error& e) {
    std::printf("%s\n", e.what());
  }
  std::printf("sales recorded: %d\n", shop.sales());
}
