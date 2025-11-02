#include "libtiles/pm/writer.h"

#include "absl/cleanup/cleanup.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "sqlite3.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace pm = libtiles::pm;

ABSL_FLAG(std::string, input_path, "", "Input file path");
ABSL_FLAG(std::string, output_path, "tiles.pmtiles", "Output file path");
ABSL_FLAG(std::string, metadata_path, "", "Metadata file path (optional)");

int main(int argc, char** argv)
{
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();

    std::string inputPath = absl::GetFlag(FLAGS_input_path);
    std::string outputPath = absl::GetFlag(FLAGS_output_path);
    std::string metadataPath = absl::GetFlag(FLAGS_metadata_path);
    CHECK(!inputPath.empty() && std::filesystem::exists(inputPath));
    CHECK(!outputPath.empty() && !std::filesystem::exists(outputPath));

    sqlite3* db = nullptr;
    absl::Cleanup dbClose = [&db]() { sqlite3_close(db); };

    if (sqlite3_open_v2(inputPath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        LOG(ERROR) << sqlite3_errmsg(db);
        return 1;
    }

    sqlite3_stmt* stmt = nullptr;
    absl::Cleanup stmtFinalize = [&stmt]() { sqlite3_finalize(stmt); };

    const auto QUERY = "SELECT tile_column, tile_row, zoom_level, tile_data FROM tiles;";
    if (sqlite3_prepare_v2(db, QUERY, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG(ERROR) << sqlite3_errmsg(db);
        return 1;
    }

    std::string metadata;
    if (!metadataPath.empty()) {
        std::ifstream metadataStream{metadataPath, std::ios::binary};
        metadata = {std::istreambuf_iterator<char>(metadataStream), {}};
    }

    // TODO(eak1mov): fill header metadata from database

    auto writer = pm::createWriter(pm::WriterParams{
        .filePath = outputPath,
        .metadata = metadata,
    });

    LOG(INFO) << "Writing tiles...";

    while (true) {
        int stepRc = sqlite3_step(stmt);
        if (stepRc == SQLITE_DONE) {
            break;
        }
        if (stepRc != SQLITE_ROW) {
            LOG(ERROR) << sqlite3_errmsg(db);
            return 1;
        }

        pm::TileId tileId{
            .x = static_cast<uint32_t>(sqlite3_column_int64(stmt, 0)),
            .y = static_cast<uint32_t>(sqlite3_column_int64(stmt, 1)),
            .z = static_cast<uint32_t>(sqlite3_column_int64(stmt, 2)),
        };
        tileId.y = (1U << tileId.z) - 1 - tileId.y; // fix mapbox tiling scheme (XYZ -> TMS)

        std::string_view tileData{
            static_cast<const char*>(sqlite3_column_blob(stmt, 3)),
            static_cast<size_t>(sqlite3_column_bytes(stmt, 3))};

        writer->writeTile(tileId, tileData);

        LOG_EVERY_N_SEC(INFO, 10) << "Processed " << COUNTER << " tiles";
    }

    if (sqlite3_reset(stmt) != SQLITE_OK) {
        LOG(ERROR) << sqlite3_errmsg(db);
        return 1;
    }

    sqlite3_close(db);
    db = nullptr;

    LOG(INFO) << "Writing index...";

    writer->finalize();

    LOG(INFO) << "Done!";

    return 0;
}
