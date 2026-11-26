#include "JoltObjectLayers.h"

JoltObjectLayerPairFilter::JoltObjectLayerPairFilter()
	: JPH::ObjectLayerPairFilterTable(JoltObjectLayers::Count)
{
	EnableCollision(JoltObjectLayers::Static, JoltObjectLayers::Character);
	EnableCollision(JoltObjectLayers::Static, JoltObjectLayers::Dynamic);
	EnableCollision(JoltObjectLayers::Dynamic, JoltObjectLayers::Dynamic);
}
