#pragma once

#include "utils.hpp"
#include "settings.hpp"
#include <iostream>
#include <string>

namespace norb {


  /**
   * @class NaivePersistentMemory
   * @brief An RAII helper class for storing unstructured config data.
   */
  class NaivePersistentMemory {
  private:
    using pos_t_ = unsigned long;
    template <typename val_t_> struct RAII_Tracker;

    static NaivePersistentMemory &get_instance() {
      static NaivePersistentMemory instance;
      return instance;
    }

    static std::string file_path;
    std::fstream fconfig;
    bool write_only;
    pos_t_ global_cur = 0;

    NaivePersistentMemory() {
      filesystem::fassert(file_path);
      fconfig = std::fstream(file_path,
                             std::ios::binary | std::ios::in | std::ios::out);
      write_only = filesystem::is_empty(fconfig);
    }
    ~NaivePersistentMemory() { fconfig.close(); }

    template <typename val_t_> struct RAII_Tracker {
    private:
      pos_t_ cur;

    public:
      val_t_ val;
      explicit RAII_Tracker(const val_t_ &default_value) : val(default_value) {
        auto &singleton = get_instance();
        cur = singleton.global_cur;
        singleton.global_cur += sizeof(pos_t_);
        if (!singleton.write_only) {
          // read from config
          singleton.fconfig.seekg(cur);
          filesystem::binary_read(singleton.fconfig, val);
        }
        assert(singleton.fconfig.good());
      }

      ~RAII_Tracker() {
        auto &singleton = get_instance();
        singleton.fconfig.seekp(cur);
        filesystem::binary_write(singleton.fconfig, val);
      }
    };

  public:
    template <typename val_t_> using tracker_t_ = RAII_Tracker<val_t_>;

    static void set_file_path(const std::string &path) { file_path = path; }

    template <typename val_t_, typename... Args>
    static RAII_Tracker<val_t_> track(Args &&...args) {
      return RAII_Tracker<val_t_>(val_t_(std::forward<Args>(args)...));
    }
  };

  inline std::string NaivePersistentMemory::file_path = settings::NPMEM_FILE_NAME;
} // namespace norb
