#include "test.hpp"

#include "caf/all.hpp"

using namespace caf;

class exception_testee : public event_based_actor {
 public:
  exception_testee() {
    set_exception_handler([](const std::exception_ptr&) -> optional<uint32_t> {
      return exit_reason::user_defined + 2;
    });
  }
  behavior make_behavior() override {
    return {
      others() >> [] {
        throw std::runtime_error("whatever");
      }
    };
  }
};

void test_custom_exception_handler() {
  auto handler = [](const std::exception_ptr& eptr) -> optional<uint32_t> {
    try {
      std::rethrow_exception(eptr);
    }
    catch (std::runtime_error&) {
      return exit_reason::user_defined;
    }
    catch (...) {
      // "fall through"
    }
    return exit_reason::user_defined + 1;
  };
  scoped_actor self;
  auto testee1 = self->spawn<monitored>([=](event_based_actor* eb_self) {
    eb_self->set_exception_handler(handler);
    throw std::runtime_error("ping");
  });
  auto testee2 = self->spawn<monitored>([=](event_based_actor* eb_self) {
    eb_self->set_exception_handler(handler);
    throw std::logic_error("pong");
  });
  auto testee3 = self->spawn<exception_testee, monitored>();
  self->send(testee3, "foo");
  // receive all down messages
  auto i = 0;
  self->receive_for(i, 3)(
    [&](const down_msg& dm) {
      if (dm.source == testee1) {
        CAF_CHECK_EQUAL(dm.reason, exit_reason::user_defined);
      }
      else if (dm.source == testee2) {
        CAF_CHECK_EQUAL(dm.reason, exit_reason::user_defined + 1);
      }
      else if (dm.source == testee3) {
        CAF_CHECK_EQUAL(dm.reason, exit_reason::user_defined + 2);
      }
      else {
        CAF_CHECK(false); // report error
      }
    }
  );
}

int main() {
  CAF_TEST(test_custom_exception_handler);
  test_custom_exception_handler();
  return CAF_TEST_RESULT();
}
