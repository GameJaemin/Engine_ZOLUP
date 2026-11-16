#include "GLTFLoader.h"

#include "Core/FileHelper.h"
#include "Core/Log.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector3.h"
#include "GLTFDocument.h"
#include "Mesh/MeshVertex.h"
#include "Mesh/StaticMesh.h"

#include "rapidjson/document.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
	// glTF는 미터, 엔진은 센티미터 단위다.
	constexpr float MetersToCentimeters = 100.0f;

	/**
	 * 엔진 로컬 좌표의 삼각형 위치와 UV에서 정규화하지 않은 U 방향을 구한다.
	 * normal은 사용하지 않는다. UV가 누락되었는지는 호출자가 따로 판단해야 한다.
	 * 유효하지 않은 값, 퇴화한 삼각형 또는 표현할 수 없는 결과는 false로 처리하며 outTangent를 유지한다.
	 * 모든 검사를 통과하면 outTangent를 채우고 true를 반환한다. 아직 로더에서 호출하지 않는다.
	 */
	bool CalculateTriangleTangent(const MeshVertex& v0, const MeshVertex& v1, const MeshVertex& v2, Vector3& outTangent)
	{
		// v0에서 나머지 두 정점으로 향하는 공간상의 변이다.
		const Vector3 edge1 = v1.position - v0.position;
		const Vector3 edge2 = v2.position - v0.position;
		const Vector3 triangleAreaVector = edge1.Cross(edge2);
		if (!std::isfinite(triangleAreaVector.x) || !std::isfinite(triangleAreaVector.y) || !std::isfinite(triangleAreaVector.z)) { return false; }
		if (triangleAreaVector.x == 0.0f && triangleAreaVector.y == 0.0f && triangleAreaVector.z == 0.0f) { return false; }

		const float edge1LengthSquared = edge1.Dot(edge1);
		const float edge2LengthSquared = edge2.Dot(edge2);
		const float triangleAreaVectorLengthSquared = triangleAreaVector.Dot(triangleAreaVector);
		if (!std::isfinite(edge1LengthSquared) || !std::isfinite(edge2LengthSquared) || !std::isfinite(triangleAreaVectorLengthSquared)) { return false; }

		constexpr float TriangleParallelTolerance = 1e-6f;
		const double triangleParallelThreshold = static_cast<double>(TriangleParallelTolerance) * TriangleParallelTolerance * edge1LengthSquared * edge2LengthSquared;
		if (triangleAreaVectorLengthSquared <= triangleParallelThreshold) { return false; }

		// 각 공간 변에 대응하는 UV 변화량이다.
		const float deltaU1 = v1.uv.x - v0.uv.x;
		const float deltaV1 = v1.uv.y - v0.uv.y;
		const float deltaU2 = v2.uv.x - v0.uv.x;
		const float deltaV2 = v2.uv.y - v0.uv.y;

		// UV의 두 변이 만드는 부호 있는 평행사변형 넓이다.
		const float uvDeterminant = deltaU1 * deltaV2 - deltaV1 * deltaU2;
		if (uvDeterminant == 0.0f) { return false; }

		const float uvEdge1LengthSquared = deltaU1 * deltaU1 + deltaV1 * deltaV1;
		const float uvEdge2LengthSquared = deltaU2 * deltaU2 + deltaV2 * deltaV2;
		if (!std::isfinite(uvDeterminant) || !std::isfinite(uvEdge1LengthSquared) || !std::isfinite(uvEdge2LengthSquared)) { return false; }

		// UV 크기와 무관하게 두 변이 거의 평행한지 비교한다. 비교식 양쪽의 곱셈은 double로 수행한다.
		constexpr float UVParallelTolerance = 1e-6f;
		const double uvParallelThreshold = static_cast<double>(UVParallelTolerance) * UVParallelTolerance * uvEdge1LengthSquared * uvEdge2LengthSquared;
		if (static_cast<double>(uvDeterminant) * uvDeterminant <= uvParallelThreshold) { return false; }

		const Vector3 triangleTangent = (edge1 * deltaV2 - edge2 * deltaV1) / uvDeterminant;
		if (!std::isfinite(triangleTangent.x) || !std::isfinite(triangleTangent.y) || !std::isfinite(triangleTangent.z)) { return false; }
		if (triangleTangent.x == 0.0f && triangleTangent.y == 0.0f && triangleTangent.z == 0.0f) { return false; }

		outTangent = triangleTangent;
		return true;
	}

	/**
	 * 삼각형별 tangent를 정점에 모아 vertices의 tangent를 채운다. indices는 읽기만 한다.
	 * 모든 정점의 계산이 성공한 뒤 tangent를 저장하고 true를 반환한다. 실패하면 vertices를 유지한다.
	 * hasTexcoords가 false이면 정점 번호 검사 후 삼각형 계산을 생략하고 정점별 대체 방향을 만든다.
	 */
	bool GenerateTangents(std::vector<MeshVertex>& vertices, const std::vector<std::uint32_t>& indices, bool hasTexcoords)
	{
		if (indices.size() % 3 != 0) { return false; }

		std::vector<Vector3> tangentSums(vertices.size());

		for (std::size_t triangleStart = 0; triangleStart < indices.size(); triangleStart += 3)
		{
			const std::uint32_t vertexIndex0 = indices[triangleStart];
			const std::uint32_t vertexIndex1 = indices[triangleStart + 1];
			const std::uint32_t vertexIndex2 = indices[triangleStart + 2];
			if (vertexIndex0 >= vertices.size() || vertexIndex1 >= vertices.size() || vertexIndex2 >= vertices.size()) { return false; }
			if (!hasTexcoords) { continue; }

			Vector3 triangleTangent;
			// 방향을 구할 수 없는 삼각형은 누적하지 않고 다음 삼각형으로 넘어간다.
			if (!CalculateTriangleTangent(vertices[vertexIndex0], vertices[vertexIndex1], vertices[vertexIndex2], triangleTangent)) { continue; }

			tangentSums[vertexIndex0] += triangleTangent;
			tangentSums[vertexIndex1] += triangleTangent;
			tangentSums[vertexIndex2] += triangleTangent;
		}

		for (std::size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex)
		{
			Vector3 normal = vertices[vertexIndex].normal;
			const float normalLengthSquared = normal.Dot(normal);
			if (!std::isfinite(normalLengthSquared) || normalLengthSquared <= 0.0f) { return false; }

			// 투영식은 단위 법선을 전제로 하므로 지역 복사본만 정규화한다.
			normal = normal / std::sqrt(normalLengthSquared);

			Vector3 tangent = tangentSums[vertexIndex];
			if (!std::isfinite(tangent.x) || !std::isfinite(tangent.y) || !std::isfinite(tangent.z)) { return false; }

			const float normalProjection = normal.Dot(tangent);
			tangent -= normal * normalProjection;

			const float tangentLengthSquared = tangent.Dot(tangent);
			if (!std::isfinite(tangentLengthSquared)) { return false; }

			if (tangentLengthSquared > 0.0f)
			{
				tangent = tangent / std::sqrt(tangentLengthSquared);
			}
			else
			{
				const float normalAbsX = std::abs(normal.x);
				const float normalAbsY = std::abs(normal.y);
				const float normalAbsZ = std::abs(normal.z);

				// 법선과 가장 덜 나란한 축을 골라 직교화 결과가 영벡터가 되는 것을 피한다.
				Vector3 referenceAxis;
				if (normalAbsX <= normalAbsY && normalAbsX <= normalAbsZ)
				{
					referenceAxis = Vector3{ 1.0f, 0.0f, 0.0f };
				}
				else if (normalAbsY <= normalAbsZ)
				{
					referenceAxis = Vector3{ 0.0f, 1.0f, 0.0f };
				}
				else
				{
					referenceAxis = Vector3{ 0.0f, 0.0f, 1.0f };
				}

				tangent = referenceAxis - normal * normal.Dot(referenceAxis);
				tangent = tangent / std::sqrt(tangent.Dot(tangent));
			}

			// 누적 배열을 최종 방향의 임시 저장소로 재사용한다.
			tangentSums[vertexIndex] = tangent;
		}

		for (std::size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex)
		{
			vertices[vertexIndex].tangent = tangentSums[vertexIndex];
		}

		return true;
	}

	/** accessors 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. */
	std::vector<GLTFAccessor> ReadAccessors(const rapidjson::Value& document)
	{
		std::vector<GLTFAccessor> accessors;
		if (!document.HasMember("accessors")) { return accessors; }

		const rapidjson::Value::ConstArray accessorArray = document["accessors"].GetArray();
		for (const rapidjson::Value& item : accessorArray)
		{
			GLTFAccessor accessor;

			if (item.HasMember("bufferView")) { accessor.bufferView = item["bufferView"].GetUint64(); }
			if (item.HasMember("byteOffset")) { accessor.byteOffset = item["byteOffset"].GetUint64(); }
			accessor.componentType = item["componentType"].GetInt();
			if (item.HasMember("normalized")) { accessor.normalized = item["normalized"].GetBool(); }
			accessor.count = item["count"].GetUint64();
			accessor.type = item["type"].GetString();

			if (item.HasMember("max"))
			{
				const rapidjson::Value::ConstArray maxArray = item["max"].GetArray();
				for (const rapidjson::Value& value : maxArray)
				{
					accessor.max.push_back(value.GetDouble());
				}
			}

			if (item.HasMember("min"))
			{
				const rapidjson::Value::ConstArray minArray = item["min"].GetArray();
				for (const rapidjson::Value& value : minArray)
				{
					accessor.min.push_back(value.GetDouble());
				}
			}

			if (item.HasMember("name")) { accessor.name = item["name"].GetString(); }

			accessors.push_back(std::move(accessor));
		}

		return accessors;
	}

	/** animations 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. */
	std::vector<GLTFAnimation> ReadAnimations(const rapidjson::Value& document)
	{
		std::vector<GLTFAnimation> animations;
		if (!document.HasMember("animations")) { return animations; }

		const rapidjson::Value::ConstArray animationArray = document["animations"].GetArray();
		for (const rapidjson::Value& item : animationArray)
		{
			GLTFAnimation animation;

			const rapidjson::Value::ConstArray channelArray = item["channels"].GetArray();
			for (const rapidjson::Value& channelItem : channelArray)
			{
				GLTFAnimationChannel channel;

				channel.sampler = channelItem["sampler"].GetUint64();
				const rapidjson::Value& targetItem = channelItem["target"];
				if (targetItem.HasMember("node")) { channel.target.node = targetItem["node"].GetUint64(); }
				channel.target.path = targetItem["path"].GetString();

				animation.channels.push_back(std::move(channel));
			}

			const rapidjson::Value::ConstArray samplerArray = item["samplers"].GetArray();
			for (const rapidjson::Value& samplerItem : samplerArray)
			{
				GLTFAnimationSampler sampler;

				sampler.input = samplerItem["input"].GetUint64();
				if (samplerItem.HasMember("interpolation")) { sampler.interpolation = samplerItem["interpolation"].GetString(); }
				sampler.output = samplerItem["output"].GetUint64();

				animation.samplers.push_back(std::move(sampler));
			}

			if (item.HasMember("name")) { animation.name = item["name"].GetString(); }

			animations.push_back(std::move(animation));
		}

		return animations;
	}

	/** buffers 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. */
	std::vector<GLTFBuffer> ReadBuffers(const rapidjson::Value& document)
	{
		std::vector<GLTFBuffer> buffers;
		if (!document.HasMember("buffers")) { return buffers; }

		const rapidjson::Value::ConstArray bufferArray = document["buffers"].GetArray();
		for (const rapidjson::Value& item : bufferArray)
		{
			GLTFBuffer buffer;

			if (item.HasMember("uri")) { buffer.uri = item["uri"].GetString(); }
			buffer.byteLength = item["byteLength"].GetUint64();
			if (item.HasMember("name")) { buffer.name = item["name"].GetString(); }

			buffers.push_back(std::move(buffer));
		}

		return buffers;
	}

	/** bufferViews 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. */
	std::vector<GLTFBufferView> ReadBufferViews(const rapidjson::Value& document)
	{
		std::vector<GLTFBufferView> bufferViews;
		if (!document.HasMember("bufferViews")) { return bufferViews; }

		const rapidjson::Value::ConstArray bufferViewArray = document["bufferViews"].GetArray();
		for (const rapidjson::Value& item : bufferViewArray)
		{
			GLTFBufferView bufferView;

			bufferView.buffer = item["buffer"].GetUint64();
			if (item.HasMember("byteOffset")) { bufferView.byteOffset = item["byteOffset"].GetUint64(); }
			bufferView.byteLength = item["byteLength"].GetUint64();
			if (item.HasMember("byteStride")) { bufferView.byteStride = item["byteStride"].GetUint64(); }
			if (item.HasMember("target")) { bufferView.target = item["target"].GetInt(); }
			if (item.HasMember("name")) { bufferView.name = item["name"].GetString(); }

			bufferViews.push_back(std::move(bufferView));
		}

		return bufferViews;
	}

	/** images 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. */
	std::vector<GLTFImage> ReadImages(const rapidjson::Value& document)
	{
		std::vector<GLTFImage> images;
		if (!document.HasMember("images")) { return images; }

		const rapidjson::Value::ConstArray imageArray = document["images"].GetArray();
		for (const rapidjson::Value& item : imageArray)
		{
			GLTFImage image;

			if (item.HasMember("uri")) { image.uri = item["uri"].GetString(); }
			if (item.HasMember("mimeType")) { image.mimeType = item["mimeType"].GetString(); }
			if (item.HasMember("bufferView")) { image.bufferView = item["bufferView"].GetUint64(); }
			if (item.HasMember("name")) { image.name = item["name"].GetString(); }

			images.push_back(std::move(image));
		}

		return images;
	}

	/** 재질의 텍스처 참조(textureInfo) 객체 하나를 읽는다. */
	GLTFTextureInfo ReadTextureInfo(const rapidjson::Value& item)
	{
		GLTFTextureInfo textureInfo;

		textureInfo.index = item["index"].GetUint64();
		if (item.HasMember("texCoord")) { textureInfo.texCoord = item["texCoord"].GetUint64(); }

		return textureInfo;
	}

	/** 재질의 노멀 텍스처 참조(normalTextureInfo) 객체 하나를 읽는다. */
	GLTFMaterialNormalTextureInfo ReadNormalTextureInfo(const rapidjson::Value& item)
	{
		GLTFMaterialNormalTextureInfo normalTextureInfo;

		// 부모 부분(index, texCoord)만 가리키는 참조로 대입해 scale은 그대로 둔다.
		GLTFTextureInfo& textureInfo = normalTextureInfo;
		textureInfo = ReadTextureInfo(item);
		if (item.HasMember("scale")) { normalTextureInfo.scale = item["scale"].GetFloat(); }

		return normalTextureInfo;
	}

	/** 재질의 차폐 텍스처 참조(occlusionTextureInfo) 객체 하나를 읽는다. */
	GLTFMaterialOcclusionTextureInfo ReadOcclusionTextureInfo(const rapidjson::Value& item)
	{
		GLTFMaterialOcclusionTextureInfo occlusionTextureInfo;

		// 부모 부분(index, texCoord)만 가리키는 참조로 대입해 strength는 그대로 둔다.
		GLTFTextureInfo& textureInfo = occlusionTextureInfo;
		textureInfo = ReadTextureInfo(item);
		if (item.HasMember("strength")) { occlusionTextureInfo.strength = item["strength"].GetFloat(); }

		return occlusionTextureInfo;
	}

	/** materials 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. pbrMetallicRoughness, normalTexture 및 occlusionTexture 외의 항목은 읽지 않는다. */
	std::vector<GLTFMaterial> ReadMaterials(const rapidjson::Value& document)
	{
		std::vector<GLTFMaterial> materials;
		if (!document.HasMember("materials")) { return materials; }

		const rapidjson::Value::ConstArray materialArray = document["materials"].GetArray();
		for (const rapidjson::Value& item : materialArray)
		{
			GLTFMaterial material;

			if (item.HasMember("name")) { material.name = item["name"].GetString(); }
			if (item.HasMember("alphaMode")) { material.alphaMode = item["alphaMode"].GetString(); }

			if (item.HasMember("pbrMetallicRoughness"))
			{
				const rapidjson::Value& pbrItem = item["pbrMetallicRoughness"];
				GLTFMaterialPBRMetallicRoughness& pbr = material.pbrMetallicRoughness;

				if (pbrItem.HasMember("baseColorFactor"))
				{
					const rapidjson::Value::ConstArray factorArray = pbrItem["baseColorFactor"].GetArray();
					for (rapidjson::SizeType i = 0; i < 4; ++i)
					{
						pbr.baseColorFactor[i] = factorArray[i].GetFloat();
					}
				}

				if (pbrItem.HasMember("baseColorTexture")) { pbr.baseColorTexture = ReadTextureInfo(pbrItem["baseColorTexture"]); }
				if (pbrItem.HasMember("metallicFactor")) { pbr.metallicFactor = pbrItem["metallicFactor"].GetFloat(); }
				if (pbrItem.HasMember("roughnessFactor")) { pbr.roughnessFactor = pbrItem["roughnessFactor"].GetFloat(); }
				if (pbrItem.HasMember("metallicRoughnessTexture")) { pbr.metallicRoughnessTexture = ReadTextureInfo(pbrItem["metallicRoughnessTexture"]); }
			}

			if (item.HasMember("normalTexture")) { material.normalTexture = ReadNormalTextureInfo(item["normalTexture"]); }
			if (item.HasMember("occlusionTexture")) { material.occlusionTexture = ReadOcclusionTextureInfo(item["occlusionTexture"]); }

			materials.push_back(std::move(material));
		}

		return materials;
	}

	/** meshes 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. */
	std::vector<GLTFMesh> ReadMeshes(const rapidjson::Value& document)
	{
		std::vector<GLTFMesh> meshes;
		if (!document.HasMember("meshes")) { return meshes; }

		const rapidjson::Value::ConstArray meshArray = document["meshes"].GetArray();
		for (const rapidjson::Value& item : meshArray)
		{
			GLTFMesh mesh;

			const rapidjson::Value::ConstArray primitiveArray = item["primitives"].GetArray();
			for (const rapidjson::Value& primitiveItem : primitiveArray)
			{
				GLTFMeshPrimitive primitive;

				const rapidjson::Value::ConstObject attributeObject = primitiveItem["attributes"].GetObject();
				for (const rapidjson::Value::Member& attribute : attributeObject)
				{
					primitive.attributes[attribute.name.GetString()] = attribute.value.GetUint64();
				}
				if (primitiveItem.HasMember("indices")) { primitive.indices = primitiveItem["indices"].GetUint64(); }
				if (primitiveItem.HasMember("material")) { primitive.material = primitiveItem["material"].GetUint64(); }
				if (primitiveItem.HasMember("mode")) { primitive.mode = primitiveItem["mode"].GetInt(); }

				mesh.primitives.push_back(std::move(primitive));
			}

			if (item.HasMember("name")) { mesh.name = item["name"].GetString(); }

			meshes.push_back(std::move(mesh));
		}

		return meshes;
	}

	/** nodes 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. */
	std::vector<GLTFNode> ReadNodes(const rapidjson::Value& document)
	{
		std::vector<GLTFNode> nodes;
		if (!document.HasMember("nodes")) { return nodes; }

		const rapidjson::Value::ConstArray nodeArray = document["nodes"].GetArray();
		for (const rapidjson::Value& item : nodeArray)
		{
			GLTFNode node;

			if (item.HasMember("children"))
			{
				const rapidjson::Value::ConstArray childArray = item["children"].GetArray();
				for (const rapidjson::Value& child : childArray)
				{
					node.children.push_back(child.GetUint64());
				}
			}

			if (item.HasMember("skin")) { node.skin = item["skin"].GetUint64(); }

			if (item.HasMember("matrix"))
			{
				const rapidjson::Value::ConstArray matrixArray = item["matrix"].GetArray();
				std::array<float, 16> matrix = {};
				for (rapidjson::SizeType i = 0; i < 16; ++i)
				{
					matrix[i] = matrixArray[i].GetFloat();
				}
				node.matrix = matrix;
			}

			if (item.HasMember("mesh")) { node.mesh = item["mesh"].GetUint64(); }

			if (item.HasMember("rotation"))
			{
				const rapidjson::Value::ConstArray rotationArray = item["rotation"].GetArray();
				for (rapidjson::SizeType i = 0; i < 4; ++i)
				{
					node.rotation[i] = rotationArray[i].GetFloat();
				}
			}

			if (item.HasMember("scale"))
			{
				const rapidjson::Value::ConstArray scaleArray = item["scale"].GetArray();
				for (rapidjson::SizeType i = 0; i < 3; ++i)
				{
					node.scale[i] = scaleArray[i].GetFloat();
				}
			}

			if (item.HasMember("translation"))
			{
				const rapidjson::Value::ConstArray translationArray = item["translation"].GetArray();
				for (rapidjson::SizeType i = 0; i < 3; ++i)
				{
					node.translation[i] = translationArray[i].GetFloat();
				}
			}

			if (item.HasMember("name")) { node.name = item["name"].GetString(); }

			nodes.push_back(std::move(node));
		}

		return nodes;
	}

	/** skins 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. */
	std::vector<GLTFSkin> ReadSkins(const rapidjson::Value& document)
	{
		std::vector<GLTFSkin> skins;
		if (!document.HasMember("skins")) { return skins; }

		const rapidjson::Value::ConstArray skinArray = document["skins"].GetArray();
		for (const rapidjson::Value& item : skinArray)
		{
			GLTFSkin skin;

			if (item.HasMember("inverseBindMatrices")) { skin.inverseBindMatrices = item["inverseBindMatrices"].GetUint64(); }
			if (item.HasMember("skeleton")) { skin.skeleton = item["skeleton"].GetUint64(); }

			const rapidjson::Value::ConstArray jointArray = item["joints"].GetArray();
			for (const rapidjson::Value& joint : jointArray)
			{
				skin.joints.push_back(joint.GetUint64());
			}

			if (item.HasMember("name")) { skin.name = item["name"].GetString(); }

			skins.push_back(std::move(skin));
		}

		return skins;
	}

	/** textures 배열을 읽는다. 배열이 없으면 빈 배열을 돌려준다. sampler는 읽지 않는다. */
	std::vector<GLTFTexture> ReadTextures(const rapidjson::Value& document)
	{
		std::vector<GLTFTexture> textures;
		if (!document.HasMember("textures")) { return textures; }

		const rapidjson::Value::ConstArray textureArray = document["textures"].GetArray();
		for (const rapidjson::Value& item : textureArray)
		{
			GLTFTexture texture;

			if (item.HasMember("source")) { texture.source = item["source"].GetUint64(); }
			if (item.HasMember("name")) { texture.name = item["name"].GetString(); }

			textures.push_back(std::move(texture));
		}

		return textures;
	}

	/** 각 buffer의 uri가 가리키는 파일을 읽어 bufferData를 채운다. 외부 파일 uri만 지원한다. 실패하면 bufferData를 바꾸지 않는다. */
	bool LoadBufferData(const std::filesystem::path& gltfPath, GLTFDocument& gltfDocument)
	{
		std::vector<std::vector<std::byte>> bufferData;
		for (const GLTFBuffer& buffer : gltfDocument.buffers)
		{
			// .glb 안의 데이터(uri 없음)와 base64 data URI는 아직 지원하지 않는다.
			if (buffer.uri.empty() || buffer.uri.starts_with("data:")) { return false; }

			// uri는 .gltf 파일이 있는 폴더 기준 상대 경로이며 UTF-8이다.
			const std::filesystem::path bufferPath = gltfPath.parent_path() / std::filesystem::path(std::u8string(buffer.uri.begin(), buffer.uri.end()));

			std::vector<std::byte> bytes;
			if (!FileHelper::ReadAllBytes(bufferPath, bytes)) { return false; }

			// 파일이 byteLength보다 작으면 accessor가 범위 밖을 읽게 된다.
			if (bytes.size() < buffer.byteLength) { return false; }

			bufferData.push_back(std::move(bytes));
		}

		gltfDocument.bufferData = std::move(bufferData);

		return true;
	}

	/** accessor의 type 문자열(SCALAR, VEC2 ...)이 뜻하는 숫자 개수. 모르는 값이면 0 */
	std::size_t GetComponentCount(const std::string& type)
	{
		if (type == "SCALAR") { return 1; }
		if (type == "VEC2") { return 2; }
		if (type == "VEC3") { return 3; }
		if (type == "VEC4") { return 4; }
		if (type == "MAT2") { return 4; }
		if (type == "MAT3") { return 9; }
		if (type == "MAT4") { return 16; }

		return 0;
	}

	/**
	 * accessor 값들이 바이트 배열의 어디에 있는지 찾는다. elementSize는 값 하나의 바이트 수다.
	 * bufferView가 없거나 번호가 범위를 벗어나거나 값이 버퍼 밖에 있으면 출력값을 유지하고 false를 반환한다.
	 */
	bool FindAccessorElements(const GLTFDocument& gltfDocument, const GLTFAccessor& accessor, std::size_t elementSize, const std::byte*& outFirstElement, std::size_t& outStride)
	{
		// bufferView가 없는(값이 모두 0인) accessor는 아직 지원하지 않는다.
		if (!accessor.bufferView.has_value()) { return false; }

		// 번호들은 파일에 적힌 값이라, 배열 범위를 벗어나면 잘못된 메모리를 읽게 되므로 쓰기 전에 확인한다.
		const std::size_t bufferViewIndex = accessor.bufferView.value();
		if (bufferViewIndex >= gltfDocument.bufferViews.size()) { return false; }

		// accessor -> bufferView -> buffer 순서로 값이 들어 있는 바이트를 찾아간다.
		const GLTFBufferView& bufferView = gltfDocument.bufferViews[bufferViewIndex];
		if (bufferView.buffer >= gltfDocument.bufferData.size()) { return false; }

		const std::vector<std::byte>& bytes = gltfDocument.bufferData[bufferView.buffer];

		// i번째 값은 start + i * stride에서 시작한다. byteStride가 없으면 값이 빈틈없이 붙어 있다.
		const std::size_t stride = bufferView.byteStride.value_or(elementSize);
		const std::size_t start = bufferView.byteOffset + accessor.byteOffset;

		// 마지막 값의 끝이 버퍼 안에 있어야 한다.
		if (accessor.count > 0 && start + stride * (accessor.count - 1) + elementSize > bytes.size()) { return false; }

		outFirstElement = bytes.data() + start;
		outStride = stride;

		return true;
	}

	/** float accessor의 값을 숫자 배열로 읽는다. 값 하나는 componentCount개의 float이며 배열에 차례로 담긴다. */
	bool ReadFloatAccessor(const GLTFDocument& gltfDocument, std::size_t accessorIndex, std::size_t componentCount, std::vector<float>& outValues)
	{
		// 번호들은 파일에 적힌 값이라, 배열 범위를 벗어나면 잘못된 메모리를 읽게 되므로 쓰기 전에 확인한다.
		if (accessorIndex >= gltfDocument.accessors.size()) { return false; }

		const GLTFAccessor& accessor = gltfDocument.accessors[accessorIndex];

		// float(5126)이고 숫자 개수가 맞아야 한다.
		if (accessor.componentType != 5126 || GetComponentCount(accessor.type) != componentCount) { return false; }

		const std::size_t elementSize = sizeof(float) * componentCount;
		const std::byte* firstElement = nullptr;
		std::size_t stride = 0;
		if (!FindAccessorElements(gltfDocument, accessor, elementSize, firstElement, stride)) { return false; }

		std::vector<float> values(accessor.count * componentCount);
		for (std::size_t i = 0; i < accessor.count; ++i)
		{
			const std::byte* sourceElementBytes = firstElement + stride * i;
			float* destinationComponents = &values[i * componentCount];

			// 정렬과 타입 규칙 문제 없이 바이트를 float로 옮기려고 memcpy를 쓴다.
			std::memcpy(destinationComponents, sourceElementBytes, elementSize);
		}

		outValues = std::move(values);

		return true;
	}

	/** 인덱스 accessor(SCALAR, uint8/uint16/uint32)의 값을 uint32로 넓혀 읽는다. */
	bool ReadIndexAccessor(const GLTFDocument& gltfDocument, std::size_t accessorIndex, std::vector<std::uint32_t>& outIndices)
	{
		// ----------
		// 인덱스 형식 검사 및 읽기 위치 확인
		// ----------
		// 번호들은 파일에 적힌 값이라, 배열 범위를 벗어나면 잘못된 메모리를 읽게 되므로 쓰기 전에 확인한다.
		if (accessorIndex >= gltfDocument.accessors.size()) { return false; }

		const GLTFAccessor& accessor = gltfDocument.accessors[accessorIndex];

		// 인덱스는 숫자 하나(SCALAR)다.
		if (accessor.type != "SCALAR") { return false; }

		// componentType에 따라 인덱스 하나의 바이트 수가 다르다. 5121 uint8, 5123 uint16, 5125 uint32
		std::size_t indexSize = 0;
		if (accessor.componentType == 5121) { indexSize = 1; }
		else if (accessor.componentType == 5123) { indexSize = 2; }
		else if (accessor.componentType == 5125) { indexSize = 4; }
		else { return false; }

		const std::byte* firstElement = nullptr;
		std::size_t stride = 0;
		if (!FindAccessorElements(gltfDocument, accessor, indexSize, firstElement, stride)) { return false; }

		// ----------
		// 인덱스를 uint32 배열로 변환
		// ----------
		std::vector<std::uint32_t> indices(accessor.count);
		for (std::size_t i = 0; i < accessor.count; ++i)
		{
			const std::byte* sourceElementBytes = firstElement + stride * i;

			// 작은 타입은 같은 크기의 변수로 복사한 뒤 uint32로 넓힌다.
			if (indexSize == 1)
			{
				std::uint8_t index = 0;
				std::memcpy(&index, sourceElementBytes, sizeof(index));
				indices[i] = index;
			}
			else if (indexSize == 2)
			{
				std::uint16_t index = 0;
				std::memcpy(&index, sourceElementBytes, sizeof(index));
				indices[i] = index;
			}
			else
			{
				std::memcpy(&indices[i], sourceElementBytes, sizeof(std::uint32_t));
			}
		}

		outIndices = std::move(indices);

		return true;
	}

	/** JOINTS accessor(VEC4, 정규화하지 않은 uint8 또는 uint16)의 값을 uint16으로 넓혀 읽는다. 값 하나는 숫자 4개이며 배열에 차례로 담긴다. */
	bool ReadJointAccessor(const GLTFDocument& gltfDocument, std::size_t accessorIndex, std::vector<std::uint16_t>& outJoints)
	{
		// 번호들은 파일에 적힌 값이라, 배열 범위를 벗어나면 잘못된 메모리를 읽게 되므로 쓰기 전에 확인한다.
		if (accessorIndex >= gltfDocument.accessors.size()) { return false; }

		const GLTFAccessor& accessor = gltfDocument.accessors[accessorIndex];

		// 뼈 번호는 정점마다 4개(VEC4)이고, 비율이 아니라 번호이므로 정규화하지 않는다.
		if (accessor.type != "VEC4" || accessor.normalized) { return false; }

		// 숫자 하나의 바이트 수. 5121 uint8, 5123 uint16
		std::size_t componentSize = 0;
		if (accessor.componentType == 5121) { componentSize = 1; }
		else if (accessor.componentType == 5123) { componentSize = 2; }
		else { return false; }

		const std::size_t elementSize = componentSize * 4;
		const std::byte* firstElement = nullptr;
		std::size_t stride = 0;
		if (!FindAccessorElements(gltfDocument, accessor, elementSize, firstElement, stride)) { return false; }

		// 정점마다 숫자 4개를 꺼내 uint16으로 넓힌다. 정점 안의 숫자는 빈틈없이 붙어 있다.
		std::vector<std::uint16_t> joints(accessor.count * 4);
		for (std::size_t i = 0; i < accessor.count; ++i)
		{
			const std::byte* sourceElementBytes = firstElement + stride * i;
			for (std::size_t c = 0; c < 4; ++c)
			{
				const std::byte* sourceComponentBytes = sourceElementBytes + componentSize * c;
				if (componentSize == 1)
				{
					std::uint8_t joint = 0;
					std::memcpy(&joint, sourceComponentBytes, sizeof(joint));
					joints[i * 4 + c] = joint;
				}
				else
				{
					std::memcpy(&joints[i * 4 + c], sourceComponentBytes, sizeof(std::uint16_t));
				}
			}
		}

		outJoints = std::move(joints);

		return true;
	}

	/** WEIGHTS accessor(VEC4, float 또는 정규화한 uint8, uint16)의 값을 float로 읽는다. 값 하나는 숫자 4개이며 배열에 차례로 담긴다. */
	bool ReadWeightAccessor(const GLTFDocument& gltfDocument, std::size_t accessorIndex, std::vector<float>& outWeights)
	{
		// 번호들은 파일에 적힌 값이라, 배열 범위를 벗어나면 잘못된 메모리를 읽게 되므로 쓰기 전에 확인한다.
		if (accessorIndex >= gltfDocument.accessors.size()) { return false; }

		const GLTFAccessor& accessor = gltfDocument.accessors[accessorIndex];

		// 가중치는 정점마다 4개(VEC4)다.
		if (accessor.type != "VEC4") { return false; }

		// 숫자 하나의 바이트 수와, 정수를 0~1 비율로 바꿀 때 나눌 최댓값. float는 정규화하지 않고 정수는 반드시 정규화한다.
		std::size_t componentSize = 0;
		float integerMax = 0.0f;
		if (accessor.componentType == 5126 && !accessor.normalized) { componentSize = 4; }
		else if (accessor.componentType == 5121 && accessor.normalized) { componentSize = 1; integerMax = 255.0f; }
		else if (accessor.componentType == 5123 && accessor.normalized) { componentSize = 2; integerMax = 65535.0f; }
		else { return false; }

		const std::size_t elementSize = componentSize * 4;
		const std::byte* firstElement = nullptr;
		std::size_t stride = 0;
		if (!FindAccessorElements(gltfDocument, accessor, elementSize, firstElement, stride)) { return false; }

		// 정점마다 숫자 4개를 꺼내 float 비율로 바꾼다. 정점 안의 숫자는 빈틈없이 붙어 있다.
		std::vector<float> weights(accessor.count * 4);
		for (std::size_t i = 0; i < accessor.count; ++i)
		{
			const std::byte* sourceElementBytes = firstElement + stride * i;
			for (std::size_t c = 0; c < 4; ++c)
			{
				const std::byte* sourceComponentBytes = sourceElementBytes + componentSize * c;
				float& weight = weights[i * 4 + c];
				if (componentSize == 4)
				{
					std::memcpy(&weight, sourceComponentBytes, sizeof(float));
				}
				else if (componentSize == 1)
				{
					std::uint8_t value = 0;
					std::memcpy(&value, sourceComponentBytes, sizeof(value));
					weight = value / integerMax;
				}
				else
				{
					std::uint16_t value = 0;
					std::memcpy(&value, sourceComponentBytes, sizeof(value));
					weight = value / integerMax;
				}
			}
		}

		outWeights = std::move(weights);

		return true;
	}

	/** glTF 좌표계(오른손, m)의 이동, 회전, 크기를 엔진 좌표계(왼손, cm)로 바꾼다. 노드와 애니메이션 키가 함께 쓴다. */
	Transform ConvertTransformToEngine(const Transform& source)
	{
		Transform transform;

		// 정점 위치와 같이 x를 뒤집고 cm로 바꾼다.
		transform.position.x = -source.position.x * MetersToCentimeters;
		transform.position.y = source.position.y * MetersToCentimeters;
		transform.position.z = source.position.z * MetersToCentimeters;

		// x를 뒤집으면 Y축, Z축 기준 회전의 방향이 반대가 되므로 y와 z 성분의 부호를 바꾼다.
		transform.rotation.x = source.rotation.x;
		transform.rotation.y = -source.rotation.y;
		transform.rotation.z = -source.rotation.z;
		transform.rotation.w = source.rotation.w;

		// 크기는 방향이 없는 축별 배율이라 그대로 쓴다.
		transform.scale = source.scale;

		return transform;
	}

	/** 노드의 이동, 회전, 크기를 엔진 좌표계(왼손, cm)의 Transform으로 바꾼다. matrix로 적힌 노드는 행렬을 나눠서 바꾼다. */
	Transform ConvertNodeTransform(const GLTFNode& node)
	{
		// ----------
		// 담기: glTF 좌표 그대로의 이동, 회전, 크기
		// ----------
		// 명세상 한 노드는 matrix와 translation, rotation, scale 중 한 방식으로만 변환을 적는다.
		Transform source;
		if (node.matrix.has_value())
		{
			// glTF는 열 우선 순서라, 16개 값을 행 단위로 채우면 엔진의 행 벡터 규칙 행렬이 된다.
			const std::array<float, 16>& values = node.matrix.value();
			Matrix4x4 matrix;
			for (int row = 0; row < 4; ++row)
			{
				for (int column = 0; column < 4; ++column)
				{
					matrix.m[row][column] = values[row * 4 + column];
				}
			}

			// 행렬을 glTF 좌표 그대로의 이동, 회전, 크기로 나눈다.
			source = Transform::CreateFromMatrix(matrix);
		}
		else
		{
			source.position.x = node.translation[0];
			source.position.y = node.translation[1];
			source.position.z = node.translation[2];

			source.rotation.x = node.rotation[0];
			source.rotation.y = node.rotation[1];
			source.rotation.z = node.rotation[2];
			source.rotation.w = node.rotation[3];

			source.scale.x = node.scale[0];
			source.scale.y = node.scale[1];
			source.scale.z = node.scale[2];
		}

		return ConvertTransformToEngine(source);
	}

	/** .gltf 파일과 외부 버퍼를 읽어 glTF 원본 그대로의 문서를 채운다. 실패하면 출력값을 유지하고 false를 반환한다. */
	bool LoadDocument(const std::filesystem::path& path, GLTFDocument& outDocument)
	{
		std::string gltfText;
		if (!FileHelper::ReadAllText(path, gltfText)) { return false; }

		rapidjson::Document document;
		document.Parse(gltfText.data(), gltfText.size());
		if (document.HasParseError() || !document.IsObject()) { return false; }

		GLTFDocument gltfDocument;
		gltfDocument.accessors = ReadAccessors(document);
		gltfDocument.animations = ReadAnimations(document);
		gltfDocument.buffers = ReadBuffers(document);
		gltfDocument.bufferViews = ReadBufferViews(document);
		gltfDocument.images = ReadImages(document);
		gltfDocument.materials = ReadMaterials(document);
		gltfDocument.meshes = ReadMeshes(document);
		gltfDocument.nodes = ReadNodes(document);
		gltfDocument.skins = ReadSkins(document);
		gltfDocument.textures = ReadTextures(document);
		if (!LoadBufferData(path, gltfDocument)) { return false; }

		outDocument = std::move(gltfDocument);

		return true;
	}

	/**
	 * 텍스처 참조를 textures -> images 순서로 따라가 이미지 uri를 찾아 채운다.
	 * TEXCOORD_0이 아니거나, 번호가 범위를 벗어나거나, source가 없거나, data URI 또는 bufferView 이미지면 출력값을 유지하고 false를 반환한다.
	 */
	bool FindTextureImageUri(const GLTFDocument& gltfDocument, const GLTFTextureInfo& textureInfo, std::string& outUri)
	{
		// 정점에는 TEXCOORD_0만 담으므로 다른 UV를 쓰는 텍스처는 바르게 붙일 수 없다.
		if (textureInfo.texCoord != 0) { return false; }
		if (textureInfo.index >= gltfDocument.textures.size()) { return false; }

		// source가 없으면 확장이 이미지를 정하는데, 확장은 지원하지 않는다.
		const GLTFTexture& texture = gltfDocument.textures[textureInfo.index];
		if (!texture.source.has_value() || texture.source.value() >= gltfDocument.images.size()) { return false; }

		// .glb 안(bufferView)이나 data URI의 이미지는 아직 지원하지 않는다.
		const GLTFImage& image = gltfDocument.images[texture.source.value()];
		if (image.uri.empty() || image.uri.starts_with("data:")) { return false; }

		outUri = image.uri;

		return true;
	}

	/**
	 * LoadDocument로 채운 원본 문서의 materialIndex번 재질을 엔진 재질 설명으로 바꿔 채운다.
	 * 계수가 0~1 밖이거나 번호가 범위를 벗어나거나, 지원하지 않는 형식의 색, 노멀 또는 금속성-거칠기 텍스처면 출력값을 유지하고 false를 반환한다.
	 * 노멀 scale이 1이 아니면 경고 로그를 남기고 1로 본다.
	 * 차폐 strength가 1이 아니면 경고 로그를 남기고 1로 본다. 차폐 텍스처를 찾지 못하면 경고 로그를 남기고 비워 둔다.
	 */
	bool ConvertMaterial(const GLTFDocument& gltfDocument, std::size_t materialIndex, GLTFModelMaterial& outMaterial)
	{
		if (materialIndex >= gltfDocument.materials.size()) { return false; }

		const GLTFMaterial& source = gltfDocument.materials[materialIndex];
		const GLTFMaterialPBRMetallicRoughness& pbr = source.pbrMetallicRoughness;

		// 명세상 계수는 모두 0~1이다. 엔진 재질도 같은 범위만 받으므로 벗어나면 잘못된 파일로 본다.
		const auto isUnitRange = [](float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; };
		if (!isUnitRange(pbr.baseColorFactor[0]) || !isUnitRange(pbr.baseColorFactor[1]) || !isUnitRange(pbr.baseColorFactor[2])) { return false; }
		if (!isUnitRange(pbr.metallicFactor) || !isUnitRange(pbr.roughnessFactor)) { return false; }

		GLTFModelMaterial material;
		material.baseColor = Vector3{ pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2] };
		material.metallic = pbr.metallicFactor;
		material.roughness = pbr.roughnessFactor;
		material.name = source.name;

		if (source.alphaMode == "BLEND")
		{
			const float alpha = pbr.baseColorFactor[3];
			if (!isUnitRange(alpha)) { return false; }

			material.alphaMode = MaterialAlphaMode::Blend;
			material.baseColorAlpha = alpha;
		}

		if (pbr.baseColorTexture.has_value())
		{
			if (!FindTextureImageUri(gltfDocument, pbr.baseColorTexture.value(), material.baseColorImageUri)) { return false; }
		}

		if (source.normalTexture.has_value())
		{
			const GLTFMaterialNormalTextureInfo& normalTexture = source.normalTexture.value();

			// 렌더러가 노멀 배율을 받지 않아 1로 그린다. 세기가 달라지므로 경고만 남기고 노멀 텍스처는 그대로 쓴다.
			if (normalTexture.scale != 1.0f)
			{
				Log::Write(LogLevel::Warning, "GLTFLoader", std::format("Material {} ('{}'): normalTexture.scale {} is not supported; using 1", materialIndex, source.name, normalTexture.scale));
			}
			if (!FindTextureImageUri(gltfDocument, normalTexture, material.normalImageUri)) { return false; }
		}

		if (pbr.metallicRoughnessTexture.has_value())
		{
			if (!FindTextureImageUri(gltfDocument, pbr.metallicRoughnessTexture.value(), material.metallicRoughnessImageUri)) { return false; }
		}

		if (source.occlusionTexture.has_value())
		{
			const GLTFMaterialOcclusionTextureInfo& occlusionTexture = source.occlusionTexture.value();

			// 렌더러가 차폐 세기를 받지 않아 1로 그린다. 노멀 scale과 같이 경고만 남긴다.
			if (occlusionTexture.strength != 1.0f)
			{
				Log::Write(LogLevel::Warning, "GLTFLoader", std::format("Material {} ('{}'): occlusionTexture.strength {} is not supported; using 1", materialIndex, source.name, occlusionTexture.strength));
			}
			// 차폐가 없어도 모양과 재질은 맞게 그려지므로, 경로를 못 찾으면 모델을 실패시키지 않고 차폐만 뺀다.
			if (!FindTextureImageUri(gltfDocument, occlusionTexture, material.occlusionImageUri))
			{
				Log::Write(LogLevel::Warning, "GLTFLoader", std::format("Material {} ('{}'): occlusionTexture is not supported; ignored", materialIndex, source.name));
			}
		}

		outMaterial = std::move(material);

		return true;
	}

	/**
	 * LoadDocument로 채운 원본 문서의 노드들을 엔진 좌표계의 배치표로 바꿔 채운다.
	 * 배치표 번호는 glTF nodes 번호와 같다. 실패하면 출력값을 유지하고 false를 반환한다.
	 */
	bool ConvertSceneNodes(const GLTFDocument& gltfDocument, std::vector<GLTFSceneNode>& outSceneNodes)
	{
		// 원본 노드 수만큼 줄을 만든다. 줄 번호가 glTF 노드 번호와 같아진다.
		std::vector<GLTFSceneNode> sceneNodes(gltfDocument.nodes.size());

		for (std::size_t i = 0; i < gltfDocument.nodes.size(); ++i)
		{
			// 메시 번호는 파일에 적힌 값이라, 범위를 벗어나면 나중에 없는 메시를 찾게 되므로 확인한다.
			if (gltfDocument.nodes[i].mesh.has_value() && gltfDocument.nodes[i].mesh.value() >= gltfDocument.meshes.size()) { return false; }

			sceneNodes[i].mesh = gltfDocument.nodes[i].mesh;
			sceneNodes[i].localTransform = ConvertNodeTransform(gltfDocument.nodes[i]);
			sceneNodes[i].name = gltfDocument.nodes[i].name;
		}

		// glTF의 자식 목록으로 각 노드의 부모와 자식 목록을 함께 채운다.
		for (std::size_t i = 0; i < gltfDocument.nodes.size(); ++i)
		{
			for (const std::size_t child : gltfDocument.nodes[i].children)
			{
				// 자식 번호는 파일에 적힌 값이라, 범위를 벗어나면 없는 노드에 쓰게 되므로 확인한다.
				if (child >= sceneNodes.size()) { return false; }

				// 명세상 노드의 부모는 하나뿐이다. 이미 부모가 적힌 자식을 다시 만나면 잘못된 파일이다.
				if (sceneNodes[child].parent.has_value()) { return false; }

				// 같은 관계를 두 방향으로 함께 적어 서로 어긋나지 않게 한다.
				sceneNodes[child].parent = i;
				sceneNodes[i].children.push_back(child);
			}
		}

		// 부모를 따라 올라가 최상위에 닿는지 확인한다. 부모는 하나뿐이라 순환이 없으면 노드 수보다 적은 걸음에 끝난다.
		for (std::size_t i = 0; i < sceneNodes.size(); ++i)
		{
			std::size_t current = i;
			std::size_t steps = 0;
			while (sceneNodes[current].parent.has_value())
			{
				current = sceneNodes[current].parent.value();
				++steps;

				// 노드 수만큼 올라가도 끝나지 않으면 같은 노드를 다시 지난 것이므로 순환이다.
				if (steps >= sceneNodes.size()) { return false; }
			}
		}

		outSceneNodes = std::move(sceneNodes);

		return true;
	}

	/** glTF 보간 문자열을 엔진 보간 방식으로 바꾼다. 지원하지 않는 값(CUBICSPLINE 등)이면 값 없음을 반환한다. */
	std::optional<AnimationInterpolation> ConvertAnimationInterpolation(const std::string& interpolation)
	{
		if (interpolation == "LINEAR") { return AnimationInterpolation::Linear; }
		if (interpolation == "STEP") { return AnimationInterpolation::Step; }
		return std::nullopt;
	}

	/** 애니메이션 하나에서 노드 하나의 키를 채널들에서 모은 것이다. 엔진 좌표로 바꾸기 전이며 키 값은 glTF 좌표 그대로다. */
	struct RawNodeTrack
	{
		// 키 시각(초). 채널이 하나도 없으면 비어 있다. 지금은 이 노드의 모든 채널이 같은 시각을 써야 한다.
		std::vector<float> times;
		// 이 노드의 모든 채널이 함께 쓰는 보간 방식. times가 비어 있으면 의미가 없다.
		AnimationInterpolation interpolation = AnimationInterpolation::Linear;
		// 키마다 숫자 3개(x, y, z)다. 이동 채널이 없으면 비어 있다.
		std::vector<float> translations;
		// 키마다 숫자 4개(x, y, z, w)다. 회전 채널이 없으면 비어 있다.
		std::vector<float> rotations;
		// 키마다 숫자 3개(x, y, z)다. 크기 채널이 없으면 비어 있다.
		std::vector<float> scales;
		// 시각이나 보간 방식이 다른 채널을 만나면 true가 된다. 이 노드는 트랙을 만들지 않는다.
		bool mismatched = false;
	};

	/**
	 * sampler의 키 시각과 키 값(키마다 숫자 componentCount개)을 읽는다. 키 값은 float accessor여야 한다.
	 * 명세를 어기면 출력값을 유지하고 false를 반환한다.
	 */
	bool ReadAnimationSamplerKeys(const GLTFDocument& gltfDocument, const GLTFAnimationSampler& sampler, std::size_t componentCount, std::vector<float>& outTimes, std::vector<float>& outValues)
	{
		// ----------
		// 키 시각: float 숫자 하나씩이며, 하나 이상이고 0 이상이며 뒤로 갈수록 커진다.
		// ----------
		std::vector<float> times;
		if (!ReadFloatAccessor(gltfDocument, sampler.input, 1, times)) { return false; }
		if (times.empty()) { return false; }

		for (std::size_t key = 0; key < times.size(); ++key)
		{
			// NaN은 크기 비교가 항상 거짓이라 아래 비교로 걸러지지 않으므로 먼저 확인한다.
			if (!std::isfinite(times[key])) { return false; }
			if (times[key] < 0.0f) { return false; }
			if (key > 0 && times[key] <= times[key - 1]) { return false; }
		}

		// ----------
		// 키 값: LINEAR와 STEP에서는 키 시각 하나에 값 하나가 대응한다.
		// ----------
		std::vector<float> values;
		if (!ReadFloatAccessor(gltfDocument, sampler.output, componentCount, values)) { return false; }
		if (values.size() != times.size() * componentCount) { return false; }

		for (const float value : values)
		{
			if (!std::isfinite(value)) { return false; }
		}

		outTimes = std::move(times);
		outValues = std::move(values);

		return true;
	}

	/** 숫자 4개씩 담긴 쿼터니언들이 모두 길이 1인지 확인한다. 파일의 소수 자릿수 오차(0.001)는 허용한다. */
	bool AreUnitQuaternions(const std::vector<float>& values)
	{
		constexpr float UnitLengthTolerance = 0.001f;

		for (std::size_t i = 0; i + 3 < values.size(); i += 4)
		{
			const float lengthSquared = values[i] * values[i] + values[i + 1] * values[i + 1] + values[i + 2] * values[i + 2] + values[i + 3] * values[i + 3];
			if (std::abs(std::sqrt(lengthSquared) - 1.0f) > UnitLengthTolerance) { return false; }
		}

		return true;
	}

	/**
	 * 노드 하나의 원본 트랙을 엔진 좌표계의 트랙으로 바꾼다. 채널이 없는 속성은 nodeLocalTransform(엔진 좌표의 노드 원래 값)을 쓴다.
	 * rawTrack의 times는 비어 있지 않고, 비어 있지 않은 키 값 목록은 times와 키 개수가 같아야 한다.
	 */
	AnimationNodeTrack ConvertAnimationNodeTrack(std::size_t node, const RawNodeTrack& rawTrack, const Transform& nodeLocalTransform)
	{
		AnimationNodeTrack track;
		track.node = node;
		track.interpolation = rawTrack.interpolation;
		track.times = rawTrack.times;
		track.values.reserve(rawTrack.times.size());

		const std::vector<float>& translations = rawTrack.translations;
		const std::vector<float>& rotations = rawTrack.rotations;
		const std::vector<float>& scales = rawTrack.scales;

		for (std::size_t key = 0; key < rawTrack.times.size(); ++key)
		{
			// glTF 좌표 그대로 담은 뒤 노드와 같은 규칙으로 엔진 좌표로 바꾼다.
			Transform source;
			if (!translations.empty()) { source.position = { translations[key * 3], translations[key * 3 + 1], translations[key * 3 + 2] }; }
			if (!rotations.empty()) { source.rotation = { rotations[key * 4], rotations[key * 4 + 1], rotations[key * 4 + 2], rotations[key * 4 + 3] }; }
			if (!scales.empty()) { source.scale = { scales[key * 3], scales[key * 3 + 1], scales[key * 3 + 2] }; }

			Transform value = ConvertTransformToEngine(source);

			// 채널이 없는 속성은 노드의 원래 값으로 채운다. 원래 값은 이미 엔진 좌표라 변환한 뒤에 넣는다.
			if (translations.empty()) { value.position = nodeLocalTransform.position; }
			if (rotations.empty()) { value.rotation = nodeLocalTransform.rotation; }
			if (scales.empty()) { value.scale = nodeLocalTransform.scale; }

			track.values.push_back(value);
		}

		return track;
	}

	/**
	 * 채널 하나의 키를 rawTracks의 대상 노드 칸에 넣는다. rawTracks는 배치표와 같은 번호다.
	 * 지원하지 않는 채널은 logPrefix를 붙여 경고하고 건너뛴다. 명세를 어기면 false를 반환한다.
	 */
	bool AddAnimationChannel(const GLTFDocument& gltfDocument, const GLTFAnimation& animation, const GLTFAnimationChannel& channel, const std::string& logPrefix, std::vector<RawNodeTrack>& rawTracks)
	{
		// 번호들은 파일에 적힌 값이라 쓰기 전에 범위를 확인한다.
		if (channel.sampler >= animation.samplers.size()) { return false; }
		const GLTFAnimationSampler& sampler = animation.samplers[channel.sampler];

		if (!channel.target.node.has_value())
		{
			Log::Write(LogLevel::Warning, "GLTFLoader", logPrefix + ": channel without node is not supported; ignored");
			return true;
		}

		const std::size_t node = channel.target.node.value();
		if (node >= rawTracks.size()) { return false; }

		// path에 맞는 키 값 목록을 고른다. 이 노드의 이동, 회전, 크기 목록 중 하나를 가리킨다.
		RawNodeTrack& rawTrack = rawTracks[node];
		std::vector<float>* keyValues = nullptr;
		std::size_t componentCount = 3;
		if (channel.target.path == "translation") { keyValues = &rawTrack.translations; }
		else if (channel.target.path == "rotation") { keyValues = &rawTrack.rotations; componentCount = 4; }
		else if (channel.target.path == "scale") { keyValues = &rawTrack.scales; }
		else
		{
			Log::Write(LogLevel::Warning, "GLTFLoader", logPrefix + std::format(": path '{}' is not supported; ignored", channel.target.path));
			return true;
		}

		const std::optional<AnimationInterpolation> interpolation = ConvertAnimationInterpolation(sampler.interpolation);
		if (!interpolation.has_value())
		{
			Log::Write(LogLevel::Warning, "GLTFLoader", logPrefix + std::format(": interpolation '{}' is not supported; ignored", sampler.interpolation));
			return true;
		}

		// 명세는 회전 키에 정수 정규화 값도 허용하지만, 지금은 float(5126)만 읽는다.
		if (sampler.output >= gltfDocument.accessors.size()) { return false; }
		if (gltfDocument.accessors[sampler.output].componentType != 5126)
		{
			Log::Write(LogLevel::Warning, "GLTFLoader", logPrefix + std::format(": non-float {} keys are not supported; ignored", channel.target.path));
			return true;
		}

		// 명세상 한 애니메이션에서 같은 노드의 같은 속성을 두 채널이 바꿀 수 없다.
		if (!keyValues->empty()) { return false; }

		// 키 값은 고른 목록에 바로 들어간다. 읽기나 검사에 실패하면 목록은 비어 있는 채로 남는다.
		std::vector<float> times;
		if (!ReadAnimationSamplerKeys(gltfDocument, sampler, componentCount, times, *keyValues)) { return false; }
		if (channel.target.path == "rotation" && !AreUnitQuaternions(rawTrack.rotations)) { return false; }

		// 노드의 첫 채널이 시각과 보간 방식을 정하고, 다음 채널부터는 같은지 비교한다.
		if (rawTrack.times.empty())
		{
			rawTrack.times = std::move(times);
			rawTrack.interpolation = interpolation.value();
		}
		else if (rawTrack.times != times || rawTrack.interpolation != interpolation.value())
		{
			rawTrack.mismatched = true;
		}

		return true;
	}

	/**
	 * animationIndex번 애니메이션을 엔진 좌표계의 트랙과 길이로 바꿔 outClip에 채운다. sceneNodes는 같은 문서로 만든 배치표다.
	 * 지원하지 않는 채널은 경고하고 건너뛴다. 명세를 어기면 출력값을 유지하고 false를 반환한다.
	 */
	bool ConvertAnimationClip(const GLTFDocument& gltfDocument, const std::vector<GLTFSceneNode>& sceneNodes, std::size_t animationIndex, AnimationClip& outClip)
	{
		const GLTFAnimation& animation = gltfDocument.animations[animationIndex];

		// 경고 로그 앞에 붙여 어느 애니메이션인지 알린다.
		const std::string logPrefix = std::format("Animation {} ('{}')", animationIndex, animation.name);

		// ----------
		// 채널을 노드별 원본 트랙에 모으기
		// ----------
		// 배치표와 같은 번호다. 채널이 없는 노드는 times가 비어 있다.
		std::vector<RawNodeTrack> rawTracks(sceneNodes.size());
		for (const GLTFAnimationChannel& channel : animation.channels)
		{
			if (!AddAnimationChannel(gltfDocument, animation, channel, logPrefix, rawTracks)) { return false; }
		}

		// ----------
		// 노드마다 엔진 트랙 만들기
		// ----------
		std::vector<AnimationNodeTrack> tracks;
		float duration = 0.0f;
		for (std::size_t node = 0; node < rawTracks.size(); ++node)
		{
			const RawNodeTrack& rawTrack = rawTracks[node];
			if (rawTrack.times.empty()) { continue; }

			if (rawTrack.mismatched)
			{
				Log::Write(LogLevel::Warning, "GLTFLoader", logPrefix + std::format(": node {} channels have different key times or interpolation; ignored", node));
				continue;
			}

			tracks.push_back(ConvertAnimationNodeTrack(node, rawTrack, sceneNodes[node].localTransform));
			duration = (std::max)(duration, rawTrack.times.back());
		}

		if (tracks.empty())
		{
			Log::Write(LogLevel::Warning, "GLTFLoader", logPrefix + ": no supported channels; clip is empty");
		}

		outClip.tracks = std::move(tracks);
		outClip.duration = duration;

		return true;
	}

	/**
	 * primitive의 JOINTS_0과 WEIGHTS_0을 정점별 스킨 가중치로 읽는다. 둘 다 없으면 빈 배열로 성공한다.
	 * 가중치는 정점마다 합이 1이 되도록 맞춘다. 명세를 어긴 값이면 출력값을 유지하고 false를 반환한다.
	 */
	bool ReadSkinWeights(const GLTFDocument& gltfDocument, std::size_t meshIndex, const GLTFMeshPrimitive& primitive, std::size_t vertexCount, std::vector<SkinWeights>& outSkinWeights)
	{
		const auto jointAttribute = primitive.attributes.find("JOINTS_0");
		const auto weightAttribute = primitive.attributes.find("WEIGHTS_0");
		const bool hasJoints = jointAttribute != primitive.attributes.end();
		const bool hasWeights = weightAttribute != primitive.attributes.end();

		// 둘 다 없으면 스킨이 없는 메시다.
		if (!hasJoints && !hasWeights)
		{
			outSkinWeights.clear();
			return true;
		}

		// 명세상 JOINTS와 WEIGHTS의 세트 수는 같아야 한다.
		if (hasJoints != hasWeights) { return false; }

		// 두 번째 세트부터는 지원하지 않는다. 명세가 한 세트만 지원해도 된다고 허용하며, 남은 4개의 합으로 다시 맞춘다.
		if (primitive.attributes.contains("JOINTS_1") || primitive.attributes.contains("WEIGHTS_1"))
		{
			Log::Write(LogLevel::Warning, "GLTFLoader", std::format("Mesh {}: JOINTS_1 and later skin sets are not supported; ignored", meshIndex));
		}

		std::vector<std::uint16_t> joints;
		if (!ReadJointAccessor(gltfDocument, jointAttribute->second, joints)) { return false; }

		std::vector<float> weights;
		if (!ReadWeightAccessor(gltfDocument, weightAttribute->second, weights)) { return false; }

		// 정점마다 숫자 4개씩이므로 다른 속성과 정점 수가 같아야 한다.
		if (joints.size() != vertexCount * 4 || weights.size() != vertexCount * 4) { return false; }

		std::vector<SkinWeights> skinWeights(vertexCount);
		for (std::size_t i = 0; i < vertexCount; ++i)
		{
			float sum = 0.0f;
			for (std::size_t c = 0; c < 4; ++c)
			{
				const float weight = weights[i * 4 + c];

				// 음수는 명세 위반이고, NaN과 무한대는 정규화와 셰이더 결과를 모두 망가뜨린다.
				if (!std::isfinite(weight) || weight < 0.0f) { return false; }

				sum += weight;
			}

			// 합이 0이면 따라갈 뼈가 없고 나눌 수도 없다. 명세상 정규화 정수의 합은 255 또는 65535여야 한다.
			if (sum <= 0.0f) { return false; }

			// 렌더러가 합으로 나누지 않으므로 여기서 합을 1로 맞춘다.
			for (std::size_t c = 0; c < 4; ++c)
			{
				skinWeights[i].joints[c] = joints[i * 4 + c];
				skinWeights[i].weights[c] = weights[i * 4 + c] / sum;
			}
		}

		outSkinWeights = std::move(skinWeights);

		return true;
	}

	/**
	 * LoadDocument로 채운 원본 문서의 meshIndex번 메시의 첫 프리미티브를 엔진 좌표계(왼손, cm)의 정점과 인덱스로 바꿔 채운다.
	 * JOINTS_0과 WEIGHTS_0이 있으면 정점별 스킨 가중치도 채운다. 이름과 경로는 유지하며, 실패하면 출력값을 유지하고 false를 반환한다.
	 */
	bool ConvertStaticMesh(const GLTFDocument& gltfDocument, std::size_t meshIndex, StaticMesh& outMeshData)
	{
		// ----------
		// 프리미티브 선택 및 정점 속성과 인덱스 읽기
		// ----------
		// 메시 번호가 범위를 벗어나면 없는 메시를 읽게 되므로 확인한다. 메시의 첫 프리미티브만 읽으며, 메시가 있으면 프리미티브도 하나 이상이다.
		if (meshIndex >= gltfDocument.meshes.size()) { return false; }
		const GLTFMeshPrimitive& primitive = gltfDocument.meshes[meshIndex].primitives[0];

		// 삼각형 목록(4, 인덱스 3개마다 삼각형 하나)만 지원한다.
		if (primitive.mode != 4) { return false; }

		// 위치가 없으면 그릴 수 없으므로 실패로 본다.
		const auto positionAttribute = primitive.attributes.find("POSITION");
		if (positionAttribute == primitive.attributes.end()) { return false; }

		// 인덱스는 glTF에서 없어도 되지만, 아직 인덱스가 있는 메시만 지원한다.
		if (!primitive.indices) { return false; }

		std::vector<float> positions;
		if (!ReadFloatAccessor(gltfDocument, positionAttribute->second, 3, positions)) { return false; }

		// 법선은 아직 직접 계산하지 않으므로 없으면 실패로 본다.
		const auto normalAttribute = primitive.attributes.find("NORMAL");
		if (normalAttribute == primitive.attributes.end()) { return false; }

		std::vector<float> normals;
		if (!ReadFloatAccessor(gltfDocument, normalAttribute->second, 3, normals)) { return false; }

		// UV는 없어도 된다. 없으면 texcoords가 비어 있다.
		std::vector<float> texcoords;
		const auto texcoordAttribute = primitive.attributes.find("TEXCOORD_0");
		if (texcoordAttribute != primitive.attributes.end())
		{
			if (!ReadFloatAccessor(gltfDocument, texcoordAttribute->second, 2, texcoords)) { return false; }
		}

		std::vector<std::uint32_t> indices;
		if (!ReadIndexAccessor(gltfDocument, primitive.indices.value(), indices)) { return false; }

		// 법선과 UV는 위치와 정점 수가 같아야 한다.
		const std::size_t vertexCount = positions.size() / 3;
		if (normals.size() != vertexCount * 3) { return false; }
		if (!texcoords.empty() && texcoords.size() != vertexCount * 2) { return false; }

		// 스킨 메시면 정점별 뼈 가중치를 읽는다. 없으면 비어 있다.
		std::vector<SkinWeights> skinWeights;
		if (!ReadSkinWeights(gltfDocument, meshIndex, primitive, vertexCount, skinWeights)) { return false; }

		// ----------
		// 엔진 좌표계로 변환 및 메시 조립
		// ----------
		// glTF는 오른손 좌표계, 미터 단위다. 엔진(왼손, cm)에 맞추려고 x를 뒤집고 위치에 100을 곱한다.
		// x를 뒤집으면 모델 앞면(glTF +Z)과 오른쪽(glTF -X)이 엔진의 +Z 앞, +X 오른쪽과 맞는다.
		std::vector<MeshVertex> vertices(vertexCount);
		for (std::size_t i = 0; i < vertexCount; ++i)
		{
			MeshVertex& vertex = vertices[i];

			const float* position = &positions[i * 3];
			vertex.position.x = -position[0] * MetersToCentimeters;
			vertex.position.y = position[1] * MetersToCentimeters;
			vertex.position.z = position[2] * MetersToCentimeters;

			// 법선은 길이 1인 방향이라 단위를 바꾸지 않고 x만 뒤집는다.
			const float* normal = &normals[i * 3];
			vertex.normal.x = -normal[0];
			vertex.normal.y = normal[1];
			vertex.normal.z = normal[2];

			// glTF와 엔진 모두 UV 원점이 왼쪽 위라 그대로 쓴다. UV가 없으면 0으로 남는다.
			if (!texcoords.empty())
			{
				const float* texcoord = &texcoords[i * 2];
				vertex.uv.x = texcoord[0];
				vertex.uv.y = texcoord[1];
			}
		}

		// 삼각형 목록이라 인덱스 3개가 삼각형 하나다.
		if (indices.size() % 3 != 0) { return false; }

		// 인덱스는 파일에 적힌 값이라, 정점 수를 넘으면 그릴 때 없는 정점을 읽게 되므로 확인한다.
		for (const std::uint32_t index : indices)
		{
			if (index >= vertexCount) { return false; }
		}

		// x를 뒤집으면 삼각형이 도는 방향도 뒤집힌다. 엔진의 앞면 약속에 맞추려고 각 삼각형의 2번째와 3번째를 바꾼다.
		for (std::size_t i = 0; i < indices.size(); i += 3)
		{
			std::swap(indices[i + 1], indices[i + 2]);
		}

		const bool hasTexcoords = texcoordAttribute != primitive.attributes.end();
		if (!GenerateTangents(vertices, indices, hasTexcoords)) { return false; }

		outMeshData.vertices = std::move(vertices);
		outMeshData.indices = std::move(indices);
		outMeshData.skinWeights = std::move(skinWeights);

		return true;
	}
}

bool GLTFLoader::LoadModel(const std::filesystem::path& path, GLTFModel& outModel)
{
	// ----------
	// glTF 문서와 외부 버퍼 읽기
	// ----------
	GLTFDocument gltfDocument;
	if (!LoadDocument(path, gltfDocument)) { return false; }

	GLTFModel model;

	// ----------
	// 재질마다 설명 만들기
	// ----------
	model.materials.resize(gltfDocument.materials.size());
	for (std::size_t i = 0; i < gltfDocument.materials.size(); ++i)
	{
		if (!ConvertMaterial(gltfDocument, i, model.materials[i])) { return false; }
	}

	// ----------
	// 메시마다 StaticMesh와 재질 번호 만들기
	// ----------
	// 이름은 glTF 메시 이름을 쓰고, 비어 있으면 "파일이름_mesh_번호"로 정한다. 경로는 받는 쪽이 정하므로 비워 둔다.
	const std::u8string fileStem = path.stem().u8string();
	model.meshes.reserve(gltfDocument.meshes.size());
	model.meshMaterials.reserve(gltfDocument.meshes.size());
	for (std::size_t i = 0; i < gltfDocument.meshes.size(); ++i)
	{
		std::string name = gltfDocument.meshes[i].name;
		if (name.empty())
		{
			name = std::string(fileStem.begin(), fileStem.end()) + "_mesh_" + std::to_string(i);
		}

		model.meshes.emplace_back(std::move(name), std::string());
		if (!ConvertStaticMesh(gltfDocument, i, model.meshes.back())) { return false; }

		// 첫 프리미티브의 재질 번호를 메시의 재질로 쓴다. 번호는 파일에 적힌 값이라 범위를 확인한다.
		const std::optional<std::size_t>& materialIndex = gltfDocument.meshes[i].primitives[0].material;
		if (materialIndex.has_value() && materialIndex.value() >= gltfDocument.materials.size()) { return false; }
		model.meshMaterials.push_back(materialIndex);
	}

	// ----------
	// 배치표 만들기
	// ----------
	if (!ConvertSceneNodes(gltfDocument, model.sceneNodes)) { return false; }

	// ----------
	// 애니메이션마다 클립 만들기
	// ----------
	// 이름은 glTF 애니메이션 이름을 쓰고, 비어 있으면 "파일이름_animation_번호"로 정한다. 경로는 받는 쪽이 정하므로 비워 둔다.
	// 트랙이 남지 않은 애니메이션도 빈 클립으로 남겨 glTF animations 번호 순서를 지킨다.
	model.animations.reserve(gltfDocument.animations.size());
	for (std::size_t i = 0; i < gltfDocument.animations.size(); ++i)
	{
		std::string name = gltfDocument.animations[i].name;
		if (name.empty())
		{
			name = std::string(fileStem.begin(), fileStem.end()) + "_animation_" + std::to_string(i);
		}

		model.animations.emplace_back(std::move(name), std::string());
		if (!ConvertAnimationClip(gltfDocument, model.sceneNodes, i, model.animations.back())) { return false; }
	}

	outModel = std::move(model);

	return true;
}
