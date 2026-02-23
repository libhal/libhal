# libhal v5

libhal v5 addresses major design concerns from v4, focusing on non-blocking operations and preventing unintended halting through coroutines.

## Changes from v4

### Foundational

- **Modules First**: Migrate from headers to C++20 modules for all code and libraries
- **Async Foundation**: All libhal interfaces return `future<T>` and accept `async_context&` as first parameter. Coroutines enable suspension points for concurrent task progress without blocking. The `async_runtime` provides `async_context` objects and accepts a transition handler callback for managing suspension points (e.g., when blocked by I/O). This system avoids global heap allocation, allowing developers to provide stack memory for each async operation.
- **Remove Timeout Parameters**: All timeout-accepting APIs replaced with coroutine-based suspension
- **Labelled ABI**: Namespace becomes `hal::inline v5` to label ABIs and support future backwards compatibility
- **Strongly Typed Units**: Migrate to mp-units library. Single precision float for most units, unsigned 32-bit integer for frequency
- **Factor Out Dependencies**: Extract generic libraries into standalone components:
  - strong_ptr
  - async_runtime/async_context
  - inplace_function (or alternative)
  - scatter_span
- **Tagged Callbacks**: All interface callbacks include unique first parameter tag type (e.g., `struct my_tag{};`) for exception disambiguation
- **Scatter Span Usage**: All span-accepting APIs must accept scatter_span

### Policies

- Use `std::pmr::polymorphic_allocator` for dynamic memory allocation during program initialization or `dyninit` time
- Factory functions use `acquire_` prefix and return type-erased smart pointers
- Widespread use of `strong_ptr` for memory management

### Interfaces

- Replace `hal::io_waiter` with C++20 coroutines and `async_context`
- Interface adjustments to accommodate coroutines
- `hal::zero_copy_serial` becomes default serial implementation

### Open Questions

1. Where should container libraries like `circular_buffer` and
   `allocated_buffer` go?

## Changes to v4 After Split

- Move v5 motor & servo interfaces into v4
- Move USB interfaces to v4
- Potential ABI break: Change v4 symbols to `hal::inline v4`
- Deprecate the following interfaces:
  - `hal::can`: too much responsibility
  - `hal::pwm`: too much responsibility
  - `hal::interrupt_pin`: Replaced by `hal::edge_triggered_interrupt`
  - (more to be determined)
- Move the following v5 interfaces into v4:
  - `hal::spi_channel`
  - `adc16`, `adc24`, `adc32`
  - `dac8`, `dac16`
  - `pwm_group_frequency`
  - `pwm_duty_cycle16`
  - Extended CAN interfaces (not `hal::can`)
  - USB interfaces
- `hal::v5::strong_ptr` will be brought into the `hal` namespace, eliminating the need to type `v5::` everywhere.
-
