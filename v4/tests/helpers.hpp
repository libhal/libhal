// Copyright 2024 - 2025 Khalil Estell and the libhal contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

struct compare_float_t
{
  float a;
  float b;
  float margin = 0.0001f;
};

/**
 * @brief Compares two single floating point values, with in a given error
 * margin.
 *
 * @param p_first first single  point value to compare
 * @param p_second second floating point value to compare
 * @param p_error_margin How precise the error of an error should be checked.
 * @return true if the two floats are equal within a margin of error
 */
bool compare_floats(compare_float_t p_compare);

// Create a test class to use with smart pointers
class test_class
{
public:
  explicit test_class(int p_value = 0)
    : m_value(p_value)
  {
    ++s_instance_count;
  }

  ~test_class()
  {
    --s_instance_count;
  }

  test_class(test_class const& p_other)
  {
    m_value = p_other.m_value;
    ++s_instance_count;
  }

  test_class& operator=(test_class const& p_other)
  {
    if (this != &p_other) {
      m_value = p_other.m_value;
      ++s_instance_count;
    }
    return *this;
  }

  test_class(test_class&& p_other) noexcept
    : m_value(p_other.m_value)
  {
  }

  test_class& operator=(test_class&& p_other) noexcept
  {
    if (this != &p_other) {
      m_value = p_other.m_value;
    }
    return *this;
  }

  [[nodiscard]] int value() const
  {
    return m_value;
  }

  void set_value(int p_value)
  {
    m_value = p_value;
  }

  // Static counter for number of instances
  inline static int s_instance_count = 0;

private:
  int m_value;
};
