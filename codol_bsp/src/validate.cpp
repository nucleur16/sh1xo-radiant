// Offline validation of a compiled map zone.

#include "world.hpp"
#include "gfx_layout.hpp"
#include "codol_mapc/server_check.hpp"

namespace codol::bsp
{
	bool validate_zone(const std::span<const std::uint8_t> zone_bytes, std::vector<std::string>& problems, std::string* summary)
	{
		const std::size_t before = problems.size();
		std::string error;
		mapc::server_collision_info info;
		if (!mapc::check_server_collision(zone_bytes, error, &info))
		{
			problems.push_back("collision ( as the dedicated server decodes it ): " + error);
		}
		else if (summary)
		{
			*summary += format("server collision ok: %u planes, %u brushes, %u sides, %u leafs, %u leaf brush nodes, %u sub models; ", info.planes, info.brushes,
				info.brush_sides, info.leafs, info.leaf_brush_nodes, info.sub_models);
		}
		validate_render(zone_bytes, problems, summary);
		return problems.size() == before;
	}
}
