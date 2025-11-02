#include "libtiles/pm/reader.h"
#include "libtiles/tileindex/tileindex.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"

#include <string>
#include <vector>

namespace pm = libtiles::pm;
namespace ti = libtiles::tileindex;

ABSL_FLAG(std::string, input_path, "", "Input file path");
ABSL_FLAG(std::string, output_path, "", "Output file path");

int main(int argc, char** argv)
{
    absl::ParseCommandLine(argc, argv);
    absl::InitializeLog();

    std::string inputPath = absl::GetFlag(FLAGS_input_path);
    std::string outputPath = absl::GetFlag(FLAGS_output_path);

    auto reader = pm::createFileReader(inputPath);

    std::vector<ti::IndexItem> indexItems;
    for (auto [tileId, tileLocation] : reader->tileLocations()) {
        indexItems.push_back({
            .x = tileId.x,
            .y = tileId.y,
            .z = tileId.z,
            .size = static_cast<uint32_t>(tileLocation.size),
            .offset = tileLocation.offset,
        });
    }

    ti::writeIndexItems(indexItems, outputPath);

    return 0;
}
