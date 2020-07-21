#ifndef AIDS_HPP_
#define AIDS_HPP_

#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace aids
{
    ////////////////////////////////////////////////////////////
    // ALGORITHM
    ////////////////////////////////////////////////////////////

    template <typename T>
    T min(T a, T b)
    {
        return a < b ? a : b;
    }

    template <typename T>
    T max(T a, T b)
    {
        return a > b ? a : b;
    }

    template <typename T>
    T clamp(T x, T low, T high)
    {
        return min(max(low, x), high);
    }

    ////////////////////////////////////////////////////////////
    // DEFER
    ////////////////////////////////////////////////////////////

    // https://www.reddit.com/r/ProgrammerTIL/comments/58c6dx/til_how_to_defer_in_c/
    template <typename F>
    struct saucy_defer {
        F f;
        saucy_defer(F f) : f(f) {}
        ~saucy_defer() { f(); }
    };

    template <typename F>
    saucy_defer<F> defer_func(F f)
    {
        return saucy_defer<F>(f);
    }

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = ::aids::defer_func([&](){code;})

    ////////////////////////////////////////////////////////////
    // Maybe
    ////////////////////////////////////////////////////////////

    template <typename T>
    struct Maybe
    {
        bool has_value;
        T unwrap;
    };

    ////////////////////////////////////////////////////////////
    // STRING_VIEW
    ////////////////////////////////////////////////////////////

    struct String_View
    {
        size_t count;
        const char *data;

        [[nodiscard]]
        String_View trim_begin(void) const
        {
            String_View view = *this;

            while (view.count != 0 && isspace(*view.data)) {
                view.data  += 1;
                view.count -= 1;
            }
            return view;
        }

        [[nodiscard]]
        String_View trim_end(void) const
        {
            String_View view = *this;

            while (view.count != 0 && isspace(*(view.data + view.count - 1))) {
                view.count -= 1;
            }
            return view;
        }

        [[nodiscard]]
        String_View trim(void) const
        {
            return trim_begin().trim_end();
        }

        void chop_back(size_t n)
        {
            count -= n < count ? n : count;
        }

        void chop(size_t n)
        {
            if (n > count) {
                data += count;
                count = 0;
            } else {
                data  += n;
                count -= n;
            }
        }

        void grow(size_t n)
        {
            count += n;
        }

        String_View chop_by_delim(char delim)
        {
            assert(data);

            size_t i = 0;
            while (i < count && data[i] != delim) i++;
            String_View result = {i, data};
            chop(i + 1);

            return result;
        }

        String_View chop_word(void)
        {
            *this = trim_begin();

            size_t i = 0;
            while (i < count && !isspace(data[i])) i++;

            String_View result = { i, data };

            count -= i;
            data  += i;

            return result;
        }

        template <typename Integer>
        Maybe<Integer> from_hex() const
        {
            Integer result = {};

            for (size_t i = 0; i < count; ++i) {
                Integer x = data[i];
                if ('0' <= x && x <= '9') {
                    x = (Integer) (x - '0');
                } else if ('a' <= x && x <= 'f') {
                    x = (Integer) (x - 'a' + 10);
                } else if ('A' <= x && x <= 'F') {
                    x = (Integer) (x - 'A' + 10);
                } else {
                    return {};
                }
                result = result * (Integer) 0x10 + x;
            }

            return {true, result};
        }

        template <typename Integer>
        Maybe<Integer> as_integer() const
        {
            Integer sign = 1;
            Integer number = {};
            String_View view = *this;

            if (view.count == 0) {
                return {};
            }

            if (*view.data == '-') {
                sign = -1;
                view.chop(1);
            }

            while (view.count) {
                if (!isdigit(*view.data)) {
                    return {};
                }
                number = number * 10 + (*view.data - '0');
                view.chop(1);
            }

            return { true, number * sign };
        }

        Maybe<float> as_float() const
        {
            char buffer[300] = {};
            memcpy(buffer, data, min(sizeof(buffer) - 1, count));
            char *endptr = NULL;
            float result = strtof(buffer, &endptr);

            if (buffer > endptr || (size_t) (endptr - buffer) != count) {
                return {};
            }

            return {true, result};
        }


        String_View subview(size_t start, size_t count) const
        {
            if (start + count <= this->count) {
                return {count, data + start};
            }

            return {};
        }

        bool operator==(String_View view) const
        {
            if (this->count != view.count) return false;
            return memcmp(this->data, view.data, this->count) == 0;
        }

        bool operator!=(String_View view) const
        {
            return !(*this == view);
        }

        bool has_prefix(String_View prefix) const
        {
            return prefix.count <= this->count
                && this->subview(0, prefix.count) == prefix;
        }
    };

    String_View operator ""_sv(const char *data, size_t count)
    {
        return {count, data};
    }

    String_View cstr_as_string_view(const char *cstr)
    {
        return {strlen(cstr), cstr};
    }

    void print1(FILE *stream, String_View view)
    {
        fwrite(view.data, 1, view.count, stream);
    }

    Maybe<String_View> read_file_as_string_view(const char *filename)
    {
        FILE *f = fopen(filename, "rb");
        if (!f) return {};
        defer(fclose(f));

        int err = fseek(f, 0, SEEK_END);
        if (err < 0) return {};

        long size = ftell(f);
        if (size < 0) return {};

        err = fseek(f, 0, SEEK_SET);
        if (err < 0) return {};

        auto data = malloc(size);
        if (!data) return {};

        size_t read_size = fread(data, 1, size, f);
        if (read_size < size && ferror(f)) return {};

        return {true, {static_cast<size_t>(size), static_cast<const char*>(data)}};
    }

    ////////////////////////////////////////////////////////////
    // PRINT
    ////////////////////////////////////////////////////////////

    void print1(FILE *stream, const char *s)
    {
        fwrite(s, 1, strlen(s), stream);
    }

    void print1(FILE *stream, char *s)
    {
        fwrite(s, 1, strlen(s), stream);
    }

    void print1(FILE *stream, char c)
    {
        fputc(c, stream);
    }

    void print1(FILE *stream, float f)
    {
        fprintf(stream, "%f", f);
    }

    void print1(FILE *stream, unsigned long long x)
    {
        fprintf(stream, "%lld", x);
    }

    template <typename ... Types>
    void print(FILE *stream, Types... args)
    {
        (print1(stream, args), ...);
    }

    template <typename T>
    void print1(FILE *stream, Maybe<T> maybe)
    {
        if (maybe.has_value) {
            print(stream, "None");
        } else {
            print(stream, "Some(", maybe.unwrap, ")");
        }
    }

    template <typename ... Types>
    void println(FILE *stream, Types... args)
    {
        (print1(stream, args), ...);
        print1(stream, '\n');
    }
}

#endif  // AIDS_HPP_
