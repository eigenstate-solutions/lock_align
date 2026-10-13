// A destructor that notifies without the obligation: under the throwing
// policy the exception leaves a destructor, so the program terminates.

#include "lock_align/ordered_condition_variable.hpp"

struct queue {
  lock_align::ordered_condition_variable<1> ready{"ready"};
  ~queue() { ready.notify_all(); }  // no obligation held
};

int main() { queue q; }
