#include "libtiles/pm/writer.h"
#include "libtiles/tileindex/tileindex.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace pm = libtiles::pm;
namespace ti = libtiles::tileindex;

ABSL_FLAG(std::string, input_index_path, "", "Input index path");
ABSL_FLAG(std::string, input_tiles_path, "", "Input tiles path");
ABSL_FLAG(std::string, output_path, "", "Output file path");

int main(int argc, char** argv)
{
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();

    std::string inputIndexPath = absl::GetFlag(FLAGS_input_index_path);
    std::string inputTilesPath = absl::GetFlag(FLAGS_input_tiles_path);
    std::string outputPath = absl::GetFlag(FLAGS_output_path);
    CHECK(!inputIndexPath.empty() && std::filesystem::exists(inputIndexPath));
    CHECK(!inputTilesPath.empty() && std::filesystem::exists(inputTilesPath));
    CHECK(!outputPath.empty() && !std::filesystem::exists(outputPath));

    auto indexItems = ti::readIndexItems(inputIndexPath);
    std::ranges::sort(indexItems, {}, [](auto&& item) { return item.offset; });

    std::ifstream tilesFile{inputTilesPath, std::ios::binary};

    auto writer = pm::createWriter(pm::WriterParams{.filePath = outputPath});

    LOG(INFO) << "Writing tiles...";

    for (const auto& item : indexItems) {
        pm::TileId tileId = {.x = item.x, .y = item.y, .z = item.z};

        std::string tileData(item.size, 0);
        tilesFile.seekg(item.offset);
        tilesFile.read(tileData.data(), tileData.size());

        writer->writeTile(tileId, tileData);

        LOG_EVERY_N_SEC(INFO, 10) << "Processed " << COUNTER << " tiles";
    }

    LOG(INFO) << "Writing index...";

    writer->finalize();
    writer.reset();

    LOG(INFO) << "Done!";

    return 0;
}
