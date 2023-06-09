#include <iostream>
#include <cmath>
#include <raylib/raylib-cpp.hpp>

#include "globals.hh"
#include "components.hh"
#include "systems.hh"
#include "util.hh"
#include "audio.hh"

// Checks for line of sight between two tile coordinates
bool line_of_sight(const TileCoord a, const TileCoord b) {
	TileCoord c = a;

	while (c.x != b.x || c.y != b.y) {
		// Move towards the destination
		c.x += sign(b.x - c.x);
		c.y += sign(b.y - c.y);

		if (c.x == b.x && c.y == b.y) return true; // Return true if the destination is reached
		if ( !tilemap.tile_in_map(c) ) return false; // Return false if it gets to the edge of the map
		if ( tilemap(c) != empty_tile ) return false; // If a tile in the way isn't empty then there is no line of sight
	}

	return false;
}

void enemy_think() {
	raylib::Vector2 player_position;
	float player_height;
	float acceleration = 1.0;

	// Find player position
	auto player_view = registry.view<const Player, const Position, const Collider>();
	for ( auto [entity, player, position, collider] : player_view.each() ) {
		player_position = position.value;
		player_height = collider.height;
		break;
	}

	const TileCoord player_coord = tilemap.world_to_tile(player_position.x, player_position.y-player_height);

	auto view = registry.view<const Enemy, Velocity, const Position, Facing, GunAttack, const Collider, AnimationState>();
	for ( auto [entity, enemy, velocity, position, facing, gun_attack, collider, animation] : view.each() ) {
		if ( !enemy.active ) continue;
		// gun.timer -= GetFrameTime();
		gun_attack.gun.update();

		// Check if the entity is in the air
		if (!collider.on_floor) {
			animation.set_state(FALL); // Play the fall animation when in the air
			continue;
		}

		// Check for line of sight to the player
		TileCoord entity_coord = tilemap.world_to_tile(position.value.x, position.value.y-collider.height);
		if ( !line_of_sight(entity_coord, player_coord) ) {
			velocity.value.x = move_towards(velocity.value.x, 0.0, acceleration);
			animation.set_state(IDLE); // Go to the idle state

			continue;
		}

		// Get the distance and direction of the player
		float distance = abs( player_position.x - position.value.x );
		int direction = sign( player_position.x - position.value.x );

		// If the player is in aggro_range, set facing and velocity to move toward them
		if ( distance > enemy.aggro_range ) continue;

		velocity.value.x = move_towards(velocity.value.x, enemy.max_speed * direction, acceleration);
		facing.direction = direction;

		// Check if the entity is on a ledge
		const TileCoord next_tile = tilemap.world_to_tile( position.value.x+(direction*(collider.width+3)/2), position.value.y+1 );

		// Don't walk off a ledge if the player is above
		if ( tilemap(next_tile) == empty_tile && player_position.y < position.value.y )
			velocity.value.x = 0;

		// If the player if in attack_range and the GunAttack timer <= 0, stop moving and attack them
		if ( distance > enemy.attack_range ) {
			animation.set_state(WALK); // When moving play the walk animation
			continue;
		}

		// Firing gun
		velocity.value.x = move_towards(velocity.value.x, 0.0, acceleration);
		if ( abs(velocity.value.x) > acceleration * 2.0 ) continue; // Wait until stopped to shoot

		animation.set_state(ATTACK);
		velocity.value.x = 0.0;
		if (gun_attack.gun.timer > 0.0) continue;

		gun_attack.gun.fire();
	}
}
