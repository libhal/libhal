# libhal

[![✅CI](https://github.com/libhal/libhal/actions/workflows/ci.yml/badge.svg)](https://github.com/libhal/libhal/actions/workflows/ci.yml)
[![GitHub stars](https://img.shields.io/github/stars/libhal/libhal.svg)](https://github.com/libhal/libhal/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/libhal/libhal.svg)](https://github.com/libhal/libhal/network)
[![GitHub issues](https://img.shields.io/github/issues/libhal/libhal.svg)](https://github.com/libhal/libhal/issues)

The foundation of the libhal ecosystem, containing the core hardware interfaces
used to make the whole ecosystem work.

## 📚 Software APIs & Usage

To learn about the available drivers and APIs see the
[API Reference](https://libhal.github.io/latest/api/)
documentation page or look at the
[`include/libhal`](https://github.com/libhal/libhal/tree/main/include/libhal)
directory.

## 🧰 Getting Started with libhal

Following the
[🚀 Getting Started](https://libhal.github.io/getting_started/)
instructions.

## 📥 Adding `libhal` to your conan project

This section assumes you are using the
[`libhal-starter`](https://github.com/libhal/libhal-starter)
project.

Add the following to your `requirements()` method to the `ConanFile` class:

```python
    def requirements(self):
          self.requires("libhal/[^4.0.0]")
```

The version number can be changed to whatever is appropriate for your
application. If you don't know what version to use, consider using the
[🚀 latest](https://github.com/libhal/libhal/releases/latest) release.

## 📥 Adding `libhal` to your library

To add libhal to your library package, do the following in the `requirements` method of your `ConanFile` object:

```python
    def requirements(self):
          self.requires("libhal/[^4.0.0]", transitive_headers=True)
```

Its important to add the `transitive_headers=True` to ensure that the libhal
headers are accessible to the library user.

## 📦 Building & Installing the Library Package

If you'd like to build and install the libhal package to your local conan
cache execute the following command:

```bash
conan create . --version=latest
```

Replace `latest` with the SEMVER version that fits the changes you've made. Or
just choose a number greater than whats been released.

> [NOTE]
> Setting the build type using the flag `-s build_type` only modifies the build
> type of the unit tests. Since this library is header only, the only files
> that are distributed to applications and libraries are the headers.
>
> It is advised to NOT use a platform profile such as `-pr lpc4078` or a cross
> compiler profile such as `-pr arm-gcc-12.3` as this will cause the unit tests
> to built for an architecture that cannot be executed on your machine. Best to
> just stick with the defaults or specify your own compiler profile yourself.

## 🌟 Package Semantic Versioning Explained

In libhal, different libraries have different requirements and expectations for
how their libraries will be used and how to interpret changes in the semantic
version of a library.

If you are not familiar with [SEMVER](https://semver.org/) you can click the
link to learn more.

### 💥 Major changes

The major number will increment in the event of:

1. An API break
2. An ABI break
3. A behavior change

We define an API break as an intentional change to the public interface, found
within the `include/` directory, that removes or changes an API in such a way
that code that previously built would no longer be capable of building.

We define an ABI break as an intentional change to the ABI of an object or
interface.

We define a "behavior change" as an intentional change to the documentation of
a public API that would change the API's behavior such that previous and later
versions of the same API would do observably different things. For example,
consider an this line of code `hal::write("Hello, World", my_callback)`. If the
description for this API was changed from, "Calls my_callback after writing the
message" to "Writes message and ignores my_callback as it an obsolete
parameter", that would be a behavioral change as code may expect that callback
to be called for the code to work correctly.

The usage of the term "intentional" means that the break or behavior change was
expected and accepted for a release. If an API break occurs on accident when it
wasn't previously desired, then such a change should be rolled back and an
alternative non-API breaking solution should be found.

You can depend on the major number to provide API, ABI, and behavioral
stability for your application. If you upgrade to a new major numbered version
of libhal, your code and applications may or may not continue to work as
expected or compile. Because of this, we try our best to not update the
major number.

### 🚀 Minor changes

The minor number will increment if a new interface, API, or type is introduced
into the public interface of libhal.

### 🐞 Patch Changes

The patch number will increment if:

1. Bug fixes that align code to the behavior of an API, improves performance
   or improves code size efficiency.
2. Any changes occur within the `/include/libhal/experimental` directory.

For now, you cannot expect ABI or API stability with anything in the
`/include/libhal/experimental` directory.

## :busts_in_silhouette: Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.
