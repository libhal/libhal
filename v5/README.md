# libhal v5

libhal v5 attempts to resolve some of the major design concerns with libhal v4.
Those issues are:

1. Time Delay Consistency: Non-blocking customizable time delays in drivers
2. Preventing unintended halting: A mechanism to prevent halting (concurrency without threading).

## Changes from v4

### Foundational

- **Modules First**: Migrate from headers to C++20 modules for all code and libraries
- **Async at the foundation**: All libhal interfaces will become coroutines meaning they return `future<T>` and accept a `async_context&` as a first parameter. By leveraging coroutines we can place suspension points into our code, allowing concurrent progress across different tasks. This system also introduces an `async_runtime` which provides `async_context` and accepts a callback for handling coroutine suspension points. For example, if a coroutine is `blocked_by::io`, this will call the transition handler, allowing your application to decide what it wants to do with that information. This systems also avoids global heap allocation and allows the app developer to provide the stack memory for each async operation.
- **Removal of all timeout functions**: All APIs that accepted timeout parameters will have those removed in place of coroutines.
- **Labelled ABI**: namespace `hal` will become `hal::inline v5`. This adds the `v5` label to our objects in tern labelling all of our ABIs. This will help will backwards compatibility into the future.
- **Strongly typed units library**: we will migrate to mp-units library instead of aliases for floats. The representation for all units will be a single precision float besides frequency which will be an unsigned 32 bit integer.
- **Factoring out dependencies**: libraries we've built that are generic and stand alone will be made into their own libraries.
  - strong ptr
  - async_runtime/async_handler (maybe workshop a new name)
  - inplace_function (or are alternative)
  - scatter_span
- **Tagged Callbacks**: For exception disambiguation purposes, all callbacks within an interface function signatures will have a first parameter that is a unique tag type like so: `struct my_tag{};`.
- Scatter span usage: all APIs that could otherwise take a span must accept a scatter span to improve

### Policies

- Adoption of `std::pmr::polymorphic_allocator` for all initial program dynamic memory allocator or at `dyninit` time.
- `acquire_` factory functions with type erased smart pointer returns
- Wide Spread Use of `strong_ptr` for memory management

### Interfaces

- Removal of `hal::io_waiter` in place of C++20 coroutines and `async_context`
- Minor changes across some interfaces to accomodate coroutines.
- `hal::zero_copy_serial` will become the defacto serial implementation.

## Changes to v4 after split

- v5 motor & servo interfaces will be moved into v4
- usb interfaces will be moved to v4
- Potential ABI break: Change v4 symbols to `hal::inline v4`
- The following interfaces will be deprecated:
  - `hal::can`: too much responsibility, meaning did and controlled too many things
  - `hal::pwm`: too much responsibility, meaning did and controlled too many things
  - (more to be determined)
- The following v5 interfaces will be moved into v4:
  - `hal::spi_channel`
  - `hal::spi_channel`
  - `adc16`,
  - `adc24`,
  - `adc32`,
  - `dac8`
  - `dac16`
  - `pwm_group_frequency`
  - `pwm_duty_cycle16`
  - Extended can interfaces (not `hal::can`)
  - usb interfaces