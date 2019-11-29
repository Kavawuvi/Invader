// SPDX-License-Identifier: GPL-3.0-only

#ifndef INVADER__BUILD__BUILD_WORKLOAD_HPP
#define INVADER__BUILD__BUILD_WORKLOAD_HPP

#include <vector>
#include <optional>
#include <string>
#include <filesystem>
#include "../hek/map.hpp"
#include "../resource/resource_map.hpp"
#include "../tag/compiled_tag.hpp"
#include "../tag/parser/parser.hpp"

namespace Invader {
    class BuildWorkload2 {
    public:
        /**
         * Compile a map
         * @param scenario          scenario tag to use
         * @param tags_directories  tags directories to use
         * @param engine_target     target a specific engine
         * @param maps_directory    maps directory to use; ignored if building a Dark Circlet map
         * @param with_index        tag index to use
         * @param no_external_tags  do not use cached tags; ignored if building a Dark Circlet map
         * @param always_index_tags always use cached tags; ignored if building a Dark Circlet map
         * @param verbose           output non-error messages to console
         * @param forge_crc         forge the CRC32 of the map
         * @param tag_data_address  address the tag data will be loaded to
         * @param rename_scenario   rename the scenario's base name (preserving the root path)
         * @param dedupe_tag_space  number of bytes desired to be deduped
         */
        static std::vector<std::byte> compile_map (
            const char *scenario,
            const std::vector<std::string> &tags_directories,
            HEK::CacheFileEngine engine_target = HEK::CacheFileEngine::CACHE_FILE_DARK_CIRCLET,
            std::string maps_directory = std::string(),
            bool no_external_tags = false,
            bool always_index_tags = false,
            bool verbose = false,
            const std::optional<std::vector<std::tuple<TagClassInt, std::string>>> &with_index = std::nullopt,
            const std::optional<std::uint32_t> &forge_crc = std::nullopt,
            const std::optional<std::uint32_t> &tag_data_address = std::nullopt,
            const std::optional<std::string> &rename_scenario = std::nullopt,
            std::size_t dedupe_tag_space = 0
        );

        /** Denotes an individual tag dependency */
        struct BuildWorkloadDependency {
            /** Index of the depended tag */
            std::size_t tag_index;

            /** Offset of the depended tag */
            std::size_t offset;

            /** It's just the tag ID */
            bool tag_id_only = false;

            bool operator==(const BuildWorkloadDependency &other) const noexcept {
                return this->tag_index == other.tag_index && this->offset == other.offset && this->tag_id_only == other.tag_id_only;
            }
        };

        /** Denotes a pointer to another struct */
        struct BuildWorkloadStructPointer {
            /** Index of the depended struct */
            std::size_t struct_index;

            /** Offset of the pointer */
            std::size_t offset;

            bool operator==(const BuildWorkloadStructPointer &other) const noexcept {
                return this->struct_index == other.struct_index && this->offset == other.offset;
            }
        };

        /** Denotes an individual tag struct */
        struct BuildWorkloadStruct {
            /** Data in the struct */
            std::vector<std::byte> data;

            /** Dependencies in the struct */
            std::vector<BuildWorkloadDependency> dependencies;

            /** Struct dependencies in the struct */
            std::vector<BuildWorkloadStructPointer> pointers;

            /** Offset of the struct in tag data if it's currently present */
            std::optional<std::size_t> offset;

            /** This struct cannot be deduped */
            bool unsafe_to_dedupe = false;

            /** BSP index */
            std::size_t bsp = 0;

            bool operator==(const BuildWorkloadStruct &other) const noexcept {
                return !this->unsafe_to_dedupe && !other.unsafe_to_dedupe && this->bsp == other.bsp && this->dependencies == other.dependencies && this->pointers == other.pointers && this->data == other.data;
            }
        };

        /** Denotes an individual tag */
        struct BuildWorkloadTag {
            /** Path of the tag */
            std::string path;

            /** Class of the tag */
            TagClassInt tag_class_int;

            /** Asset data structs */
            std::vector<std::size_t> asset_data;

            /** Index of the tag */
            std::optional<std::size_t> tag_index;

            /** Base struct index of the tag */
            std::optional<std::size_t> base_struct;
        };

        /** Structs being worked with */
        std::vector<BuildWorkloadStruct> structs;

        /** Vertices for models */
        std::vector<Parser::GBXModelVertexUncompressed> model_vertices;

        /** Indices for models */
        std::vector<std::uint16_t> model_indices;

        /** Raw data for bitmaps and sounds */
        std::vector<std::vector<std::byte>> raw_data;

        /** Tags being worked with */
        std::vector<BuildWorkloadTag> tags;

        /** BSP struct */
        std::optional<std::size_t> bsp_struct;

        /** Cache file type */
        std::optional<HEK::CacheFileType> cache_file_type;

        /**
         * Add the tag
         * @param tag_path      path of the tag
         * @param tag_class_int class of the tag
         * @return              index of the tag
         */
        std::size_t compile_tag_recursively(const char *tag_path, TagClassInt tag_class_int);

        /**
         * Dedupe all of the structs!
         * @return number of bytes saved
         */
        std::size_t dedupe_structs();

        enum ErrorType {
            /**
             * These are warnings that can be turned off. They report issues that don't affect the game, but the user may want to be made aware of them.
             */
            ERROR_TYPE_WARNING_PEDANTIC,

            /**
             * These are warnings that cannot be turned off. They report issues that do affect the game but still result in a valid cache file.
             */
            ERROR_TYPE_WARNING,

            /**
             * These are errors that prevent saving the cache file. They report issues that result in a potentially unplayable yet still technically valid cache file.
             */
            ERROR_TYPE_ERROR,

            /**
             * These are errors that halt building the cache file. They report issues that result in invalid and/or unusable cache files.
             */
            ERROR_TYPE_FATAL_ERROR
        };

        /**
         * Report an error
         * @param type      error type
         * @param error     error message
         * @param tag_index tag index
         */
        void report_error(ErrorType type, const char *error, std::optional<std::size_t> tag_index = std::nullopt);

    private:
        BuildWorkload2() = default;

        const char *scenario;
        std::size_t scenario_index;
        std::uint32_t tag_data_address;
        std::vector<std::byte> build_cache_file();
        const std::vector<std::string> *tags_directories;
        void add_tags();
        std::size_t dedupe_tag_space;
        bool hide_pedantic_warnings = false;
        std::size_t warnings = 0;
        std::size_t errors = 0;
    };

    #define REPORT_ERROR_PRINTF(workload, type, tag_index, ...) { \
        char report_error_message[256]; \
        std::snprintf(report_error_message, sizeof(report_error_message), __VA_ARGS__); \
        workload.report_error(Invader::BuildWorkload2::ErrorType::type, report_error_message, tag_index); \
    }
}

#endif