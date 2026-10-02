#include "rh29_machine_model.h"

#include <cstring>
#include <limits>

namespace eka2l1::machine::rh29 {
    namespace {
        std::uint32_t read_u32_le(const std::uint8_t *p) {
            return static_cast<std::uint32_t>(p[0])
                | (static_cast<std::uint32_t>(p[1]) << 8)
                | (static_cast<std::uint32_t>(p[2]) << 16)
                | (static_cast<std::uint32_t>(p[3]) << 24);
        }

        std::uint64_t read_value_le(const void *value, const std::size_t width) {
            const auto *bytes = static_cast<const std::uint8_t *>(value);
            std::uint64_t result = 0;
            const std::size_t limit = width > sizeof(result) ? sizeof(result) : width;
            for (std::size_t i = 0; i < limit; ++i) {
                result |= static_cast<std::uint64_t>(bytes[i]) << (i * 8);
            }
            return result;
        }
    }

    parse_result parse_rom_header(const std::uint8_t *data, const std::size_t size) {
        parse_result result{};
        if (!data || size < eka1_rom_header_size) {
            result.error = parse_error::truncated_header;
            return result;
        }

        result.header.restart_vector = read_u32_le(data + 0x7C);
        result.header.rom_base = read_u32_le(data + 0x8C);
        result.header.rom_size = read_u32_le(data + 0x90);
        result.header.rom_root_dir_list = read_u32_le(data + 0x94);
        result.header.kern_data_address = read_u32_le(data + 0x98);
        result.header.kern_limit = read_u32_le(data + 0x9C);

        if (result.header.rom_base != expected_eka1_rom_base) {
            result.error = parse_error::unexpected_rom_base;
            return result;
        }
        if (result.header.rom_size == 0) {
            result.error = parse_error::invalid_rom_size;
            return result;
        }
        if (static_cast<std::size_t>(result.header.rom_size) > size) {
            result.error = parse_error::rom_size_exceeds_file;
            return result;
        }

        const std::uint64_t end = static_cast<std::uint64_t>(result.header.rom_base)
            + static_cast<std::uint64_t>(result.header.rom_size);
        if (end > (static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1ULL)) {
            result.error = parse_error::mapped_range_overflow;
            return result;
        }

        result.ok = true;
        result.error = parse_error::none;
        return result;
    }

    strict_bus::strict_bus(const std::uint8_t *rom_data,
                           const std::size_t rom_size,
                           const std::uint32_t rom_base,
                           const std::optional<std::uint32_t> read_alias_base,
                           const std::optional<std::uint32_t> ram_base,
                           const std::size_t ram_size)
        : rom_data_(rom_data)
        , rom_size_(rom_size)
        , rom_base_(rom_base)
        , read_alias_base_(read_alias_base)
        , ram_base_(ram_base)
        , ram_data_(ram_base && ram_size ? ram_size : 0, 0)
        , ram_initialized_(ram_base && ram_size ? ram_size : 0, 0)
        , candidate_ram_probe_data_(candidate_ram_probe_size * candidate_ram_probe_loop_windows, 0)
        , candidate_bootstrap_copy_data_(candidate_bootstrap_copy_size, 0)
        , candidate_bootstrap_copy_initialized_(candidate_bootstrap_copy_size, 0)
        , candidate_bootstrap_stack_data_(candidate_bootstrap_stack_push_size, 0)
        , candidate_bootstrap_stack_initialized_(candidate_bootstrap_stack_push_size, 0)
        , candidate_bootstrap_nested_stack_data_(candidate_bootstrap_nested_stack_push_size, 0)
        , candidate_bootstrap_nested_stack_initialized_(candidate_bootstrap_nested_stack_push_size, 0)
        , candidate_post_probe_workspace_data_(candidate_post_probe_workspace_size, 0)
        , low_vector_shadow_data_(low_vector_shadow_size, 0) {
        if (rom_data_ && rom_size_ >= low_vector_shadow_size) {
            std::memcpy(low_vector_shadow_data_.data(), rom_data_, low_vector_shadow_size);
        }
    }

    bool strict_bus::range_inside_rom_mapping(const std::uint32_t address,
                                              const std::size_t width,
                                              std::size_t &offset) const {
        if (!rom_data_ || width == 0) {
            return false;
        }

        const auto check_range = [&](const std::uint32_t base) {
            if (address < base) {
                return false;
            }

            const std::uint64_t off64 = static_cast<std::uint64_t>(address) - base;
            const std::uint64_t end64 = off64 + static_cast<std::uint64_t>(width);
            if (end64 < off64 || end64 > rom_size_) {
                return false;
            }

            offset = static_cast<std::size_t>(off64);
            return true;
        };

        if (check_range(rom_base_)) {
            return true;
        }
        return read_alias_base_ && check_range(*read_alias_base_);
    }

    bool strict_bus::range_inside_ram(const std::uint32_t address,
                                      const std::size_t width,
                                      std::size_t &offset) const {
        if (!ram_base_ || ram_data_.empty() || width == 0 || address < *ram_base_) {
            return false;
        }
        const std::uint64_t off64 = static_cast<std::uint64_t>(address) - *ram_base_;
        const std::uint64_t end64 = off64 + static_cast<std::uint64_t>(width);
        if (end64 < off64 || end64 > ram_data_.size()) {
            return false;
        }
        offset = static_cast<std::size_t>(off64);
        return true;
    }

    bool strict_bus::range_inside_candidate_ram_probe(const std::uint32_t address,
                                                         const std::size_t width,
                                                         std::size_t &offset) const {
        if (width == 0) {
            return false;
        }

        for (std::size_t window = 0; window < candidate_ram_probe_loop_windows; ++window) {
            const std::uint64_t base64 = static_cast<std::uint64_t>(candidate_ram_probe_base)
                + static_cast<std::uint64_t>(candidate_ram_probe_stride) * window;
            if (address < base64) {
                continue;
            }

            const std::uint64_t local = static_cast<std::uint64_t>(address) - base64;
            const std::uint64_t end64 = local + static_cast<std::uint64_t>(width);
            if (end64 < local || end64 > candidate_ram_probe_size) {
                continue;
            }

            offset = window * candidate_ram_probe_size + static_cast<std::size_t>(local);
            return true;
        }

        return false;
    }

    bool strict_bus::range_inside_candidate_bootstrap_copy(
        const std::uint32_t address,
        const std::size_t width,
        std::size_t &offset) const {
        if (width == 0 || address < candidate_bootstrap_copy_base) {
            return false;
        }
        const std::uint64_t off64 = static_cast<std::uint64_t>(address)
            - candidate_bootstrap_copy_base;
        const std::uint64_t end64 = off64 + static_cast<std::uint64_t>(width);
        if (end64 < off64 || end64 > candidate_bootstrap_copy_data_.size()) {
            return false;
        }
        offset = static_cast<std::size_t>(off64);
        return true;
    }

    bool strict_bus::range_inside_candidate_bootstrap_stack(
        const std::uint32_t address,
        const std::size_t width,
        std::size_t &offset) const {
        if (width == 0 || address < candidate_bootstrap_stack_push_base) {
            return false;
        }
        const std::uint64_t off64 = static_cast<std::uint64_t>(address)
            - candidate_bootstrap_stack_push_base;
        const std::uint64_t end64 = off64 + static_cast<std::uint64_t>(width);
        if (end64 < off64 || end64 > candidate_bootstrap_stack_data_.size()) {
            return false;
        }
        offset = static_cast<std::size_t>(off64);
        return true;
    }

    bool strict_bus::range_inside_candidate_bootstrap_nested_stack(
        const std::uint32_t address,
        const std::size_t width,
        std::size_t &offset) const {
        if (width == 0 || address < candidate_bootstrap_nested_stack_push_base) {
            return false;
        }
        const std::uint64_t off64 = static_cast<std::uint64_t>(address)
            - candidate_bootstrap_nested_stack_push_base;
        const std::uint64_t end64 = off64 + static_cast<std::uint64_t>(width);
        if (end64 < off64 || end64 > candidate_bootstrap_nested_stack_data_.size()) {
            return false;
        }
        offset = static_cast<std::size_t>(off64);
        return true;
    }

    bool strict_bus::range_inside_candidate_post_probe_workspace(
        const std::uint32_t address,
        const std::size_t width,
        std::size_t &offset) const {
        if (width == 0 || address < candidate_post_probe_workspace_base) {
            return false;
        }
        const std::uint64_t off64 = static_cast<std::uint64_t>(address)
            - candidate_post_probe_workspace_base;
        const std::uint64_t end64 = off64 + static_cast<std::uint64_t>(width);
        if (end64 < off64 || end64 > candidate_post_probe_workspace_data_.size()) {
            return false;
        }
        offset = static_cast<std::size_t>(off64);
        return true;
    }

    bool strict_bus::range_inside_low_vector_shadow(const std::uint32_t address,
                                                    const std::size_t width,
                                                    std::size_t &offset) const {
        if (!read_alias_base_ || *read_alias_base_ != low_vector_shadow_base
            || width == 0 || address < low_vector_shadow_base) {
            return false;
        }

        const std::uint64_t off64 = static_cast<std::uint64_t>(address) - low_vector_shadow_base;
        const std::uint64_t end64 = off64 + static_cast<std::uint64_t>(width);
        if (end64 < off64 || end64 > low_vector_shadow_data_.size()) {
            return false;
        }

        offset = static_cast<std::size_t>(off64);
        return true;
    }

    void strict_bus::record_unresolved(const access_kind kind,
                                       const std::size_t width,
                                       const std::uint32_t address,
                                       const std::uint32_t pc,
                                       const std::uint32_t lr,
                                       const std::uint64_t value,
                                       const unresolved_cause cause) {
        if (first_unresolved_) {
            return;
        }

        first_unresolved_ = unresolved_access{kind, width, address, pc, lr, value, 1, cause};
    }

    bool strict_bus::read(const access_kind kind,
                          const std::uint32_t address,
                          void *out,
                          const std::size_t width,
                          const std::uint32_t pc,
                          const std::uint32_t lr) {
        if (!out) {
            record_unresolved(kind, width, address, pc, lr, 0, unresolved_cause::unmapped);
            return false;
        }

        if (out
            && flash_autoselect_active_
            && kind == access_kind::data_read
            && address == amd_reference_manufacturer_id_address
            && width == amd_reference_manufacturer_id_width) {
            const std::uint16_t id = amd_reference_manufacturer_id;
            std::memcpy(out, &id, sizeof(id));
            ++amd_reference_manufacturer_read_count_;
            return true;
        }

        if (out
            && flash_autoselect_active_
            && kind == access_kind::data_read
            && address == amd_reference_device_id_address
            && width == amd_reference_device_id_width) {
            const std::uint16_t id = amd_reference_device_id;
            std::memcpy(out, &id, sizeof(id));
            ++amd_reference_device_read_count_;
            return true;
        }

        std::size_t offset = 0;
        if (range_inside_low_vector_shadow(address, width, offset)) {
            std::memcpy(out, low_vector_shadow_data_.data() + offset, width);
            ++low_vector_shadow_read_count_;
            return true;
        }

        if (range_inside_rom_mapping(address, width, offset)) {
            std::memcpy(out, rom_data_ + offset, width);
            return true;
        }

        if (range_inside_ram(address, width, offset)) {
            for (std::size_t i = 0; i < width; ++i) {
                if (!ram_initialized_[offset + i]) {
                    record_unresolved(kind, width, address, pc, lr, 0, unresolved_cause::ram_uninitialized);
                    return false;
                }
            }
            std::memcpy(out, ram_data_.data() + offset, width);
            return true;
        }

        if (kind == access_kind::data_read
            && range_inside_candidate_bootstrap_nested_stack(address, width, offset)) {
            bool initialized = true;
            for (std::size_t i = 0; i < width; ++i) {
                if (!candidate_bootstrap_nested_stack_initialized_[offset + i]) {
                    initialized = false;
                    break;
                }
            }
            if (initialized) {
                std::memcpy(out, candidate_bootstrap_nested_stack_data_.data() + offset, width);
                ++candidate_bootstrap_nested_stack_read_count_;
                return true;
            }
        }

        if (kind == access_kind::data_read
            && range_inside_candidate_bootstrap_stack(address, width, offset)) {
            bool initialized = true;
            for (std::size_t i = 0; i < width; ++i) {
                if (!candidate_bootstrap_stack_initialized_[offset + i]) {
                    initialized = false;
                    break;
                }
            }
            if (initialized) {
                std::memcpy(out, candidate_bootstrap_stack_data_.data() + offset, width);
                ++candidate_bootstrap_stack_read_count_;
                return true;
            }
        }

        if (kind == access_kind::data_read
            && range_inside_candidate_bootstrap_copy(address, width, offset)) {
            bool initialized = true;
            for (std::size_t i = 0; i < width; ++i) {
                if (!candidate_bootstrap_copy_initialized_[offset + i]) {
                    initialized = false;
                    break;
                }
            }
            if (initialized) {
                std::memcpy(out, candidate_bootstrap_copy_data_.data() + offset, width);
                ++candidate_bootstrap_copy_read_count_;
                return true;
            }
        }

        if (kind == access_kind::data_read
            && range_inside_candidate_ram_probe(address, width, offset)) {
            std::memcpy(out, candidate_ram_probe_data_.data() + offset, width);
            ++candidate_ram_probe_read_count_;
            return true;
        }

        if (kind == access_kind::data_read
            && range_inside_candidate_post_probe_workspace(address, width, offset)) {
            std::memcpy(out, candidate_post_probe_workspace_data_.data() + offset, width);
            ++candidate_post_probe_workspace_read_count_;
            return true;
        }

        record_unresolved(kind, width, address, pc, lr, 0, unresolved_cause::unmapped);
        return false;
    }

    bool strict_bus::write(const std::uint32_t address,
                           const void *value,
                           const std::size_t width,
                           const std::uint32_t pc,
                           const std::uint32_t lr) {
        const std::uint64_t write_value = value ? read_value_le(value, width) : 0;
        if (value
            && address == observed_mmio_write_address
            && width == observed_mmio_write_width
            && write_value == observed_mmio_write_value) {
            ++observed_mmio_write_count_;
            return true;
        }

        if (value
            && address == observed_flash_command_address
            && width == observed_flash_command_width
            && write_value == observed_flash_command_value) {
            ++observed_flash_command_count_;
            return true;
        }

        if (value
            && address == observed_flash_id_entry_address
            && width == observed_flash_id_entry_width
            && write_value == observed_flash_id_entry_value) {
            ++observed_flash_id_entry_count_;
            flash_autoselect_active_ = true;
            flash_unlock_stage_ = 0;
            return true;
        }

        if (value
            && address == observed_flash_id_exit_address
            && width == observed_flash_id_exit_width
            && write_value == observed_flash_id_exit_value) {
            ++observed_flash_id_exit_count_;
            flash_autoselect_active_ = false;
            flash_unlock_stage_ = 0;
            return true;
        }

        if (value
            && !flash_autoselect_active_
            && address == observed_flash_unlock1_address
            && width == observed_flash_unlock1_width
            && write_value == observed_flash_unlock1_value) {
            ++observed_flash_unlock1_count_;
            flash_unlock_stage_ = 1;
            return true;
        }

        if (value
            && !flash_autoselect_active_
            && flash_unlock_stage_ == 1
            && address == observed_flash_unlock2_address
            && width == observed_flash_unlock2_width
            && write_value == observed_flash_unlock2_value) {
            ++observed_flash_unlock2_count_;
            flash_unlock_stage_ = 2;
            return true;
        }

        if (value
            && !flash_autoselect_active_
            && flash_unlock_stage_ == 2
            && address == observed_flash_unlock_autoselect_address
            && width == observed_flash_unlock_autoselect_width
            && write_value == observed_flash_unlock_autoselect_value) {
            ++observed_flash_unlock_autoselect_count_;
            flash_autoselect_active_ = true;
            flash_unlock_stage_ = 0;
            return true;
        }

        std::size_t offset = 0;
        if (value
            && width == low_vector_shadow_write_width
            && (address & (low_vector_shadow_write_width - 1u)) == 0
            && range_inside_low_vector_shadow(address, width, offset)) {
            std::memcpy(low_vector_shadow_data_.data() + offset, value, width);
            ++low_vector_shadow_write_count_;
            return true;
        }

        if (value && range_inside_ram(address, width, offset)) {
            std::memcpy(ram_data_.data() + offset, value, width);
            for (std::size_t i = 0; i < width; ++i) {
                if (!ram_initialized_[offset + i]) {
                    ram_initialized_[offset + i] = 1;
                    ++ram_initialized_bytes_;
                }
            }
            if (ram_write_trace_count_ < ram_write_trace_.size()) {
                auto &entry = ram_write_trace_[ram_write_trace_count_++];
                entry.address = address;
                entry.pc = pc;
                entry.lr = lr;
                entry.width_bits = static_cast<std::uint32_t>(width * 8u);
                entry.value = write_value;
            }
            ++ram_write_count_;
            return true;
        }

        const bool exact_nested_stack_push =
            pc == candidate_bootstrap_nested_stack_push_pc
            && lr == candidate_bootstrap_nested_stack_push_lr;
        const bool exact_third_stack_push =
            pc == candidate_bootstrap_third_stack_push_pc
            && lr == candidate_bootstrap_third_stack_push_lr
            && address >= candidate_bootstrap_third_stack_push_base
            && static_cast<std::uint64_t>(address) + width
                <= static_cast<std::uint64_t>(candidate_bootstrap_third_stack_top);

        if (value
            && width == candidate_bootstrap_nested_stack_write_width
            && (exact_nested_stack_push || exact_third_stack_push)
            && (address & (candidate_bootstrap_nested_stack_write_width - 1u)) == 0
            && range_inside_candidate_bootstrap_nested_stack(address, width, offset)) {
            std::memcpy(candidate_bootstrap_nested_stack_data_.data() + offset, value, width);
            for (std::size_t i = 0; i < width; ++i) {
                if (!candidate_bootstrap_nested_stack_initialized_[offset + i]) {
                    candidate_bootstrap_nested_stack_initialized_[offset + i] = 1;
                    ++candidate_bootstrap_nested_stack_initialized_bytes_;
                }
            }
            ++candidate_bootstrap_nested_stack_write_count_;
            return true;
        }

        if (value
            && width == candidate_bootstrap_stack_write_width
            && pc == candidate_bootstrap_stack_push_pc
            && lr == candidate_bootstrap_stack_push_lr
            && (address & (candidate_bootstrap_stack_write_width - 1u)) == 0
            && range_inside_candidate_bootstrap_stack(address, width, offset)) {
            std::memcpy(candidate_bootstrap_stack_data_.data() + offset, value, width);
            for (std::size_t i = 0; i < width; ++i) {
                if (!candidate_bootstrap_stack_initialized_[offset + i]) {
                    candidate_bootstrap_stack_initialized_[offset + i] = 1;
                    ++candidate_bootstrap_stack_initialized_bytes_;
                }
            }
            ++candidate_bootstrap_stack_write_count_;
            return true;
        }

        if (value
            && width == sizeof(std::uint32_t)
            && pc == candidate_bootstrap_record_loop_mutation_pc
            && lr == candidate_bootstrap_record_loop_mutation_lr
            && range_inside_candidate_bootstrap_copy(address, width, offset)) {
            const std::uint32_t first_control =
                candidate_bootstrap_record_table_base
                + static_cast<std::uint32_t>(candidate_bootstrap_record_control_offset);
            const std::uint32_t rel = address >= first_control ? address - first_control : 0xFFFFFFFFu;
            const bool control_slot =
                address >= first_control + static_cast<std::uint32_t>(candidate_bootstrap_record_stride)
                && rel % candidate_bootstrap_record_stride == 0
                && rel / candidate_bootstrap_record_stride < candidate_bootstrap_record_count;
            bool initialized = control_slot;
            for (std::size_t i = 0; initialized && i < width; ++i) {
                if (!candidate_bootstrap_copy_initialized_[offset + i]) {
                    initialized = false;
                }
            }
            if (initialized) {
                std::uint32_t old_value = 0;
                std::uint32_t observed = 0;
                std::memcpy(&old_value, candidate_bootstrap_copy_data_.data() + offset, sizeof(old_value));
                std::memcpy(&observed, value, sizeof(observed));
                const std::uint32_t expected =
                    (old_value & ~candidate_bootstrap_record_loop_clear_mask)
                    | candidate_bootstrap_record_loop_or_mask;
                const std::uint32_t low5 = old_value & 0x1Fu;
                const bool observed_low5 = low5 == 1u || low5 == 2u || low5 == 3u;
                if ((old_value & 0x80000000u) == 0
                    && observed_low5
                    && observed == expected) {
                    std::memcpy(candidate_bootstrap_copy_data_.data() + offset, value, width);
                    ++candidate_bootstrap_record_loop_mutation_count_;
                    return true;
                }
            }
        }

        if (value
            && width == candidate_bootstrap_post_copy_mutation_width
            && address == candidate_bootstrap_post_copy_mutation_address
            && pc == candidate_bootstrap_post_copy_mutation_pc
            && lr == candidate_bootstrap_post_copy_mutation_lr
            && range_inside_candidate_bootstrap_copy(address, width, offset)) {
            bool initialized = true;
            for (std::size_t i = 0; i < width; ++i) {
                if (!candidate_bootstrap_copy_initialized_[offset + i]) {
                    initialized = false;
                    break;
                }
            }
            std::uint32_t observed = 0;
            std::memcpy(&observed, value, sizeof(observed));
            if (initialized && observed == candidate_bootstrap_post_copy_mutation_value) {
                std::memcpy(candidate_bootstrap_copy_data_.data() + offset, value, width);
                ++candidate_bootstrap_post_copy_mutation_count_;
                return true;
            }
        }

        if (value
            && width == candidate_bootstrap_copy_write_width
            && pc == candidate_bootstrap_copy_pc
            && lr == candidate_bootstrap_copy_lr
            && (address & (candidate_bootstrap_copy_write_width - 1u)) == 0
            && range_inside_candidate_bootstrap_copy(address, width, offset)) {
            std::memcpy(candidate_bootstrap_copy_data_.data() + offset, value, width);
            for (std::size_t i = 0; i < width; ++i) {
                if (!candidate_bootstrap_copy_initialized_[offset + i]) {
                    candidate_bootstrap_copy_initialized_[offset + i] = 1;
                    ++candidate_bootstrap_copy_initialized_bytes_;
                }
            }
            ++candidate_bootstrap_copy_write_count_;
            return true;
        }

        if (value && range_inside_candidate_ram_probe(address, width, offset)) {
            std::memcpy(candidate_ram_probe_data_.data() + offset, value, width);
            ++candidate_ram_probe_write_count_;
            return true;
        }

        if (value && range_inside_candidate_post_probe_workspace(address, width, offset)) {
            std::memcpy(candidate_post_probe_workspace_data_.data() + offset, value, width);
            ++candidate_post_probe_workspace_write_count_;
            return true;
        }

        const auto cause = range_inside_rom_mapping(address, width, offset)
            ? unresolved_cause::rom_write
            : unresolved_cause::unmapped;
        record_unresolved(access_kind::data_write, width, address, pc, lr,
                          write_value, cause);
        return false;
    }

    const std::optional<unresolved_access> &strict_bus::first_unresolved() const {
        return first_unresolved_;
    }

    std::uint64_t strict_bus::ram_write_count() const {
        return ram_write_count_;
    }

    std::size_t strict_bus::ram_initialized_bytes() const {
        return ram_initialized_bytes_;
    }

    std::size_t strict_bus::ram_write_trace_count() const {
        return ram_write_trace_count_;
    }

    const std::array<ram_write_trace_entry, ram_write_trace_capacity> &strict_bus::ram_write_trace() const {
        return ram_write_trace_;
    }

    std::uint64_t strict_bus::observed_mmio_write_count() const {
        return observed_mmio_write_count_;
    }

    std::uint64_t strict_bus::observed_flash_command_count() const {
        return observed_flash_command_count_;
    }

    std::uint64_t strict_bus::observed_flash_id_entry_count() const {
        return observed_flash_id_entry_count_;
    }

    std::uint64_t strict_bus::amd_reference_manufacturer_read_count() const {
        return amd_reference_manufacturer_read_count_;
    }

    std::uint64_t strict_bus::amd_reference_device_read_count() const {
        return amd_reference_device_read_count_;
    }

    std::uint64_t strict_bus::observed_flash_id_exit_count() const {
        return observed_flash_id_exit_count_;
    }

    bool strict_bus::flash_autoselect_active() const {
        return flash_autoselect_active_;
    }

    std::uint64_t strict_bus::observed_flash_unlock1_count() const {
        return observed_flash_unlock1_count_;
    }

    std::uint64_t strict_bus::observed_flash_unlock2_count() const {
        return observed_flash_unlock2_count_;
    }

    std::uint64_t strict_bus::observed_flash_unlock_autoselect_count() const {
        return observed_flash_unlock_autoselect_count_;
    }

    std::uint32_t strict_bus::flash_unlock_stage() const {
        return flash_unlock_stage_;
    }

    std::uint64_t strict_bus::candidate_ram_probe_read_count() const {
        return candidate_ram_probe_read_count_;
    }

    std::uint64_t strict_bus::candidate_ram_probe_write_count() const {
        return candidate_ram_probe_write_count_;
    }

    std::uint64_t strict_bus::candidate_bootstrap_copy_read_count() const {
        return candidate_bootstrap_copy_read_count_;
    }

    std::uint64_t strict_bus::candidate_bootstrap_copy_write_count() const {
        return candidate_bootstrap_copy_write_count_;
    }

    std::size_t strict_bus::candidate_bootstrap_copy_initialized_bytes() const {
        return candidate_bootstrap_copy_initialized_bytes_;
    }

    std::uint64_t strict_bus::candidate_bootstrap_post_copy_mutation_count() const {
        return candidate_bootstrap_post_copy_mutation_count_;
    }

    std::uint64_t strict_bus::candidate_bootstrap_record_loop_mutation_count() const {
        return candidate_bootstrap_record_loop_mutation_count_;
    }

    std::uint64_t strict_bus::candidate_bootstrap_stack_read_count() const {
        return candidate_bootstrap_stack_read_count_;
    }

    std::uint64_t strict_bus::candidate_bootstrap_stack_write_count() const {
        return candidate_bootstrap_stack_write_count_;
    }

    std::size_t strict_bus::candidate_bootstrap_stack_initialized_bytes() const {
        return candidate_bootstrap_stack_initialized_bytes_;
    }

    std::uint64_t strict_bus::candidate_bootstrap_nested_stack_read_count() const {
        return candidate_bootstrap_nested_stack_read_count_;
    }

    std::uint64_t strict_bus::candidate_bootstrap_nested_stack_write_count() const {
        return candidate_bootstrap_nested_stack_write_count_;
    }

    std::size_t strict_bus::candidate_bootstrap_nested_stack_initialized_bytes() const {
        return candidate_bootstrap_nested_stack_initialized_bytes_;
    }

    std::uint64_t strict_bus::candidate_post_probe_workspace_read_count() const {
        return candidate_post_probe_workspace_read_count_;
    }

    std::uint64_t strict_bus::candidate_post_probe_workspace_write_count() const {
        return candidate_post_probe_workspace_write_count_;
    }

    std::uint64_t strict_bus::low_vector_shadow_read_count() const {
        return low_vector_shadow_read_count_;
    }

    std::uint64_t strict_bus::low_vector_shadow_write_count() const {
        return low_vector_shadow_write_count_;
    }
}
