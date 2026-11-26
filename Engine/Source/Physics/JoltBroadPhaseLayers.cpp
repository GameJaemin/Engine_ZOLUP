#include "JoltBroadPhaseLayers.h"

#include "JoltObjectLayers.h"

JoltBroadPhaseLayerMapping::JoltBroadPhaseLayerMapping()
	: JPH::BroadPhaseLayerInterfaceTable(JoltObjectLayers::Count, JoltBroadPhaseLayers::Count)
{
	MapObjectToBroadPhaseLayer(JoltObjectLayers::Static, JoltBroadPhaseLayers::Static);
	MapObjectToBroadPhaseLayer(JoltObjectLayers::Character, JoltBroadPhaseLayers::Character);
	MapObjectToBroadPhaseLayer(JoltObjectLayers::Dynamic, JoltBroadPhaseLayers::Dynamic);
}
