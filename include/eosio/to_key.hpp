#pragma once

#include <eosio/stream.hpp>

namespace eosio {

template <typename... Ts, typename S>
result<void> to_key(const std::tuple<Ts...>& obj, S& stream);

template <typename T, typename S>
result<void> to_key(const T& obj, S& stream);

template <int i, typename T, typename S>
result<void> to_key_tuple(const T& obj, S& stream) {
   if constexpr (i < std::tuple_size_v<T>) {
      auto r = to_key(std::get<i>(obj), stream);
      if (!r)
         return r.error();
      return to_key_tuple<i + 1>(obj, stream);
   }
   return outcome::success();
}

template <typename... Ts, typename S>
result<void> to_key(const std::tuple<Ts...>& obj, S& stream) {
   return to_key_tuple<0>(obj, stream);
}

template <typename S>
result<void> to_key(bool obj, S& stream) {
   return stream.write(static_cast<char>(obj ? 1 : 0));
}

template <typename UInt, typename T>
UInt float_to_key(T value) {
   static_assert(sizeof(T) == sizeof(UInt), "Expected unsigned int of the same size");
   UInt result;
   std::memcpy(&result, &value, sizeof(T));
   UInt signbit = (static_cast<UInt>(1) << (std::numeric_limits<UInt>::digits - 1));
   UInt mask    = 0;
   if (result == signbit)
      result = 0;
   if (result & signbit)
      mask = ~mask;
   return result ^ (mask | signbit);
}

template <typename T, typename S>
result<void> to_key(const T& obj, S& stream) {
   if constexpr (std::is_floating_point_v<T>) {
      if constexpr (sizeof(T) == 4) {
         return to_key(float_to_key<uint32_t>(obj), stream);
      } else {
         static_assert(sizeof(T) == 8, "Unknown floating point type");
         return to_key(float_to_key<uint64_t>(obj), stream);
      }
   } else if constexpr (std::is_integral_v<T>) {
      auto v = static_cast<std::make_unsigned_t<T>>(obj);
      v -= static_cast<std::make_unsigned_t<T>>(std::numeric_limits<T>::min());
      std::reverse(reinterpret_cast<char*>(&v), reinterpret_cast<char*>(&v + 1));
      return stream.write_raw(v);
   } else {
      result<void> r = outcome::success();
      for_each_field(obj, [&](const auto& member) {
         if (r)
            r = to_key(member, stream);
      });
      return r;
   }
}

template <typename T>
result<void> convert_to_key(const T& t, std::vector<char>& bin) {
   size_stream ss;
   auto        r = to_key(t, ss);
   if (!r)
      return r.error();
   auto orig_size = bin.size();
   bin.resize(orig_size + ss.size);
   fixed_buf_stream fbs(bin.data() + orig_size, ss.size);
   r = to_key(t, fbs);
   if (!r)
      return r.error();
   if (fbs.pos == fbs.end)
      return outcome::success();
   else
      return stream_error::underrun;
}

template <typename T>
result<std::vector<char>> convert_to_key(const T& t) {
   std::vector<char> result;
   auto              r = convert_to_key(t, result);
   if (r)
      return std::move(result);
   else
      return r.error();
}

} // namespace eosio
