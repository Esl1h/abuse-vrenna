/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See config_file.h.
 *
 *  This software was released into the Public Domain.
 */

#include "config_file.h"

#include <stdio.h>
#include <string.h>
#include "compat.h"

namespace abuse::data {

namespace {

size_t first_non_space(std::string const &s, size_t from)
{
    while (from < s.size() && (s[from] == ' ' || s[from] == '\t'))
        from++;
    return from;
}

// Does this line assign `key`, ignoring leading spaces and an optional single
// leading ';' that makes it a commented out example?
bool line_sets(std::string const &line, std::string const &key, bool &commented)
{
    size_t i = first_non_space(line, 0);
    commented = false;
    if (i < line.size() && line[i] == ';')
    {
        commented = true;
        i = first_non_space(line, i + 1);
    }

    if (line.size() - i < key.size() + 1)
        return false;
    if (strncasecmp(line.c_str() + i, key.c_str(), key.size()) != 0)
        return false;

    return line[i + key.size()] == '=';
}

}

std::string set_config_key(std::string const &text, std::string const &key,
                           std::string const &value)
{
    if (key.empty())
        return text;

    std::string assignment = key + "=" + value;
    std::string out;
    out.reserve(text.size() + assignment.size() + 1);

    bool written = false;
    bool used_comment = false;
    size_t claimed_at = 0;      // where the line taken from a comment starts
    size_t pos = 0;

    while (pos <= text.size())
    {
        size_t eol = text.find('\n', pos);
        bool last = eol == std::string::npos;
        std::string line = text.substr(pos, last ? std::string::npos : eol - pos);

        // Carriage returns survive untouched on the lines we do not rewrite;
        // on the one we do, the assignment replaces the whole line anyway.
        bool commented = false;
        if (line_sets(line, key, commented))
        {
            // The first real assignment wins. A commented example is taken
            // over only while no real one has been seen, so a file that has
            // both ends up with the real one rewritten.
            if (!written || (used_comment && !commented))
            {
                // Drop the earlier line we had claimed from a comment, by
                // where it was written: searching for its text would find
                // the last line that merely contains it.
                if (written)
                    out.erase(claimed_at, assignment.size() + 1);
                claimed_at = out.size();
                out += assignment;
                written = true;
                used_comment = commented;
            }
            else
                out += line;
        }
        else
            out += line;

        if (last)
            break;
        out += '\n';
        pos = eol + 1;
    }

    if (!written)
    {
        if (!out.empty() && out.back() != '\n')
            out += '\n';
        out += assignment;
        out += '\n';
    }

    return out;
}

std::string set_config_lines(std::string const &text, std::string const &key,
                             std::vector<std::string> const &values)
{
    if (key.empty())
        return text;

    std::string block;
    for (std::string const &v : values)
    {
        block += key;
        block += '=';
        block += v;
        block += '\n';
    }

    std::string out;
    out.reserve(text.size() + block.size());

    bool written = false;
    size_t pos = 0;

    while (pos <= text.size())
    {
        size_t eol = text.find('\n', pos);
        bool last = eol == std::string::npos;
        std::string line = text.substr(pos, last ? std::string::npos : eol - pos);

        bool commented = false;
        if (line_sets(line, key, commented) && !commented)
        {
            // Only real assignments go. A commented one is documentation here,
            // not a disabled setting: createRCFile writes the syntax of bind=
            // as a comment, and deleting it would cost the player the
            // explanation of the very thing they just changed.
            if (!written)
            {
                out += block;
                written = true;
            }
            // The newline that followed this line is already in `block`.
            if (last)
                break;
            pos = eol + 1;
            continue;
        }

        out += line;
        if (last)
            break;
        out += '\n';
        pos = eol + 1;
    }

    if (!written)
    {
        if (!out.empty() && out.back() != '\n')
            out += '\n';
        out += block;
    }

    return out;
}

namespace {

bool read_file(char const *path, std::string &text)
{
    FILE *in = fopen(path, "rb");
    if (!in)
        return false;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        text.append(buf, n);
    fclose(in);
    return true;
}

bool write_file(char const *path, std::string const &text)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    fclose(f);
    return ok;
}

}

bool save_config_lines(char const *path, char const *key,
                       std::vector<std::string> const &values)
{
    if (!path || !key)
        return false;

    std::string text;
    read_file(path, text);
    return write_file(path, set_config_lines(text, key, values));
}

bool save_config_key(char const *path, char const *key, char const *value)
{
    if (!path || !key || !value)
        return false;

    std::string text;
    FILE *in = fopen(path, "rb");
    if (in)
    {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
            text.append(buf, n);
        fclose(in);
    }

    std::string out = set_config_key(text, key, value);

    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    bool ok = fwrite(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

}
