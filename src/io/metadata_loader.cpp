#include "io/metadata_loader.h"
#include "platform/runtime_paths.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef PIXELSCOPE_METADATA_BRIDGE_PATH
#define PIXELSCOPE_METADATA_BRIDGE_PATH "metadata_bridge"
#endif

#ifndef PIXELSCOPE_METADATA_BRIDGE_FILENAME
#define PIXELSCOPE_METADATA_BRIDGE_FILENAME "metadata_bridge"
#endif

namespace pixelscope::io
{

  namespace
  {

    constexpr std::array<char, 8> kMetadataMagic = {'P', 'S', 'M', 'E', 'T', 'A', '1', '\0'};

    template <typename T>
    bool read_value(std::ifstream &stream, T &value)
    {
      stream.read(reinterpret_cast<char *>(&value), sizeof(T));
      return stream.good();
    }

    std::uint32_t read_u32_le(std::ifstream &stream, bool &ok)
    {
      std::uint32_t value = 0;
      ok = read_value(stream, value);
      return value;
    }

    std::string read_string(std::ifstream &stream, bool &ok)
    {
      const auto size = read_u32_le(stream, ok);
      if (!ok)
      {
        return {};
      }

      std::string value(size, '\0');
      if (size > 0)
      {
        stream.read(value.data(), static_cast<std::streamsize>(size));
        ok = stream.good();
      }
      return value;
    }

    std::filesystem::path temp_output_path(const char *suffix)
    {
      namespace fs = std::filesystem;

      std::error_code error_code;
      fs::path directory = fs::temp_directory_path(error_code);
      if (error_code)
      {
        directory = fs::current_path(error_code);
      }
      if (directory.empty())
      {
        directory = ".";
      }

      const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
      for (int attempt = 0; attempt < 64; ++attempt)
      {
        const auto candidate = directory /
                               ("pixelscope_metadata_" + std::to_string(timestamp) + "_" +
                                std::to_string(attempt) + suffix);
        if (!fs::exists(candidate, error_code))
        {
          return candidate;
        }
      }
      return directory / ("pixelscope_metadata_fallback" + std::string(suffix));
    }

    std::string shell_quote(const std::string &value)
    {
#ifdef _WIN32
      std::string quoted = "\"";
      for (char c : value)
      {
        if (c == '"')
        {
          quoted += "\\\"";
        }
        else
        {
          quoted += c;
        }
      }
      quoted += "\"";
      return quoted;
#else
      std::string quoted = "'";
      for (char c : value)
      {
        if (c == '\'')
        {
          quoted += "'\"'\"'";
        }
        else
        {
          quoted += c;
        }
      }
      quoted += "'";
      return quoted;
#endif
    }

    struct ScopedFileCleanup
    {
      std::filesystem::path path;

      ~ScopedFileCleanup()
      {
        if (!path.empty())
        {
          std::error_code error_code;
          std::filesystem::remove(path, error_code);
        }
      }
    };

    bool run_metadata_bridge(const std::string &input_path, const std::filesystem::path &output_path)
    {
      const auto bridge_path = pixelscope::platform::resolve_companion_binary(
          PIXELSCOPE_METADATA_BRIDGE_FILENAME,
          PIXELSCOPE_METADATA_BRIDGE_PATH);

#ifdef _WIN32
      // Use CreateProcess directly to avoid the overhead of spawning cmd.exe via std::system().
      std::wstring cmdline = L"\"" + bridge_path.wstring() + L"\" " +
                             L"\"" + std::filesystem::path(input_path).wstring() + L"\" " +
                             L"\"" + output_path.wstring() + L"\"";

      STARTUPINFOW si{};
      si.cb = sizeof(si);
      PROCESS_INFORMATION pi{};

      if (!CreateProcessW(
              nullptr,
              cmdline.data(),
              nullptr,
              nullptr,
              FALSE,
              CREATE_NO_WINDOW,
              nullptr,
              nullptr,
              &si,
              &pi))
      {
        return false;
      }

      WaitForSingleObject(pi.hProcess, INFINITE);

      DWORD exit_code = 0;
      const bool got_exit_code = GetExitCodeProcess(pi.hProcess, &exit_code);
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
      return got_exit_code && exit_code == 0;
#else
      const std::string command = shell_quote(bridge_path.string()) + " " +
                                  shell_quote(input_path) + " " +
                                  shell_quote(output_path.string());
      return std::system(command.c_str()) == 0;
#endif
    }

    std::vector<pixelscope::core::MetadataEntry> parse_metadata_payload(const std::filesystem::path &payload_path)
    {
      std::ifstream stream(payload_path, std::ios::binary);
      if (!stream.is_open())
      {
        return {};
      }

      std::array<char, 8> magic{};
      stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
      if (stream.gcount() != static_cast<std::streamsize>(magic.size()) || magic != kMetadataMagic)
      {
        return {};
      }

      bool ok = true;
      const auto version = read_u32_le(stream, ok);
      if (!ok || version != 1)
      {
        return {};
      }

      const auto entry_count = read_u32_le(stream, ok);
      if (!ok)
      {
        return {};
      }

      std::vector<pixelscope::core::MetadataEntry> entries;
      entries.reserve(static_cast<std::size_t>(entry_count));
      for (std::uint32_t index = 0; index < entry_count; ++index)
      {
        auto label = read_string(stream, ok);
        auto value = read_string(stream, ok);
        if (!ok || label.empty() || value.empty())
        {
          return {};
        }
        entries.push_back({.label = std::move(label), .value = std::move(value)});
      }

      return entries;
    }

  } // namespace

  std::vector<pixelscope::core::MetadataEntry> load_embedded_metadata(const std::string &path)
  {
    const auto bridge_path = pixelscope::platform::resolve_companion_binary(
        PIXELSCOPE_METADATA_BRIDGE_FILENAME,
        PIXELSCOPE_METADATA_BRIDGE_PATH);
    if (!std::filesystem::exists(bridge_path))
    {
      return {};
    }

    const auto payload_path = temp_output_path(".bin");
    ScopedFileCleanup payload_cleanup{payload_path};
    if (!run_metadata_bridge(path, payload_path))
    {
      return {};
    }

    return parse_metadata_payload(payload_path);
  }

  void merge_metadata_entries(
      std::vector<pixelscope::core::MetadataEntry> &destination,
      std::vector<pixelscope::core::MetadataEntry> source)
  {
    for (auto &entry : source)
    {
      if (entry.label.empty() || entry.value.empty())
      {
        continue;
      }

      bool already_present = false;
      for (const auto &existing : destination)
      {
        if (existing.label == entry.label && existing.value == entry.value)
        {
          already_present = true;
          break;
        }
      }
      if (!already_present)
      {
        destination.push_back(std::move(entry));
      }
    }
  }

} // namespace pixelscope::io
