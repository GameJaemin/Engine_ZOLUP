#include "RenderScene.h"

#include "CameraViewInfo.h"
#include "Component/AmbientLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/SkinComponent.h"
#include "Component/SkyboxComponent.h"
#include "Component/SpotLightComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "Core/Math/MathUtility.h"
#include "Core/Math/Matrix4x4.h"
#include "GameObject.h"
#include "HakkoResources.h"
#include "Mesh/StaticMesh.h"
#include "Renderer/Material.h"
#include "Renderer/ViewportRect.h"

#include "DataHub.h"
#include "DataHub_Interface.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{
	constexpr std::uint8_t PointLightType = 2;
	constexpr std::uint8_t SpotLightType = 3;
	constexpr float DirectionalShadowDistanceCm = 5000.0f;

	void ClearDrawRequests(std::uint8_t layer)
	{
		Hakko::DataHub::ClearOpaqueDrawRequests(layer);
		Hakko::DataHub::ClearTranslucentDrawRequests(layer);
		Hakko::DataHub::ClearShadowDrawRequests(layer);
	}

	DirectX::BoundingBox CalculateLocalBounds(const StaticMesh& mesh)
	{
		if (mesh.vertices.empty()) { return DirectX::BoundingBox{ {}, {} }; }

		Vector3 minimum = mesh.vertices.front().position;
		Vector3 maximum = minimum;
		for (const MeshVertex& vertex : mesh.vertices)
		{
			const Vector3& position = vertex.position;
			minimum.x = (std::min)(minimum.x, position.x);
			minimum.y = (std::min)(minimum.y, position.y);
			minimum.z = (std::min)(minimum.z, position.z);
			maximum.x = (std::max)(maximum.x, position.x);
			maximum.y = (std::max)(maximum.y, position.y);
			maximum.z = (std::max)(maximum.z, position.z);
		}

		const Vector3 center{ std::midpoint(minimum.x, maximum.x), std::midpoint(minimum.y, maximum.y), std::midpoint(minimum.z, maximum.z) };
		const Vector3 extents = (maximum - minimum) * 0.5f;
		return DirectX::BoundingBox{ { center.x, center.y, center.z }, { extents.x, extents.y, extents.z } };
	}

	float LightBrightness(const Hakko::DataHub::DataBox_RenderLight& light)
	{
		const float luminance = light.color.x * 0.2126f + light.color.y * 0.7152f + light.color.z * 0.0722f;
		return std::isfinite(luminance) ? (std::max)(luminance, 0.0f) : 0.0f;
	}

	void SelectLocalLightsForView(std::vector<Hakko::DataHub::DataBox_RenderLight>& lights, const CameraViewInfo& camera, const Matrix4x4& view, float aspectRatio)
	{
		constexpr std::size_t maxLights = RENDER_LIGHT_MAX - 1;
		if (lights.size() <= maxLights) { return; }

		struct RankedLight
		{
			std::size_t index;
			bool touchesView;
			float score;
		};

		const bool isPerspective = camera.projectionMode == CameraProjectionMode::Perspective;
		const float halfHeight = isPerspective ? std::tan(Math::DegreesToRadians(camera.verticalFovDegrees * 0.5f)) : camera.orthographicHeight * 0.5f;
		const float halfWidth = halfHeight * aspectRatio;
		const float sideScaleX = isPerspective ? std::hypot(1.0f, halfWidth) : 1.0f;
		const float sideScaleY = isPerspective ? std::hypot(1.0f, halfHeight) : 1.0f;
		std::vector<RankedLight> ranked;
		ranked.reserve(lights.size());

		for (std::size_t index = 0; index < lights.size(); ++index)
		{
			const Hakko::DataHub::DataBox_RenderLight& light = lights[index];
			const Vector3 position{ light.position.x, light.position.y, light.position.z };
			const float viewX = position.x * view.m[0][0] + position.y * view.m[1][0] + position.z * view.m[2][0] + view.m[3][0];
			const float viewY = position.x * view.m[0][1] + position.y * view.m[1][1] + position.z * view.m[2][1] + view.m[3][1];
			const float viewZ = position.x * view.m[0][2] + position.y * view.m[1][2] + position.z * view.m[2][2] + view.m[3][2];
			const float extent = isPerspective ? viewZ : 1.0f;
			// 범위 구가 여섯 시야 평면 중 하나의 바깥에 완전히 있을 때만 시야 밖으로 본다.
			const bool touchesDepth = viewZ + light.range >= camera.nearClipDistance && viewZ - light.range <= camera.farClipDistance;
			const bool touchesX = std::abs(viewX) - halfWidth * extent <= light.range * sideScaleX;
			const bool touchesY = std::abs(viewY) - halfHeight * extent <= light.range * sideScaleY;
			const Vector3 offset = position - camera.position;
			const float distance = std::sqrt(offset.Dot(offset));
			const float coverage = std::isfinite(distance) ? light.range / (std::max)(light.range, distance) : 0.0f;
			ranked.push_back({ index, touchesDepth && touchesX && touchesY, LightBrightness(light) * coverage });
		}

		std::partial_sort(ranked.begin(), ranked.begin() + maxLights, ranked.end(), [](const RankedLight& lhs, const RankedLight& rhs)
		{
			if (lhs.touchesView != rhs.touchesView) { return lhs.touchesView; }
			if (lhs.score != rhs.score) { return lhs.score > rhs.score; }
			return lhs.index < rhs.index;
		});

		std::vector<Hakko::DataHub::DataBox_RenderLight> selected;
		selected.reserve(maxLights);
		for (std::size_t index = 0; index < maxLights; ++index)
		{
			selected.push_back(lights[ranked[index].index]);
		}
		lights.swap(selected);
	}

	float EstimateLightContribution(const Hakko::DataHub::DataBox_RenderLight& light, const DirectX::BoundingBox& bounds)
	{
		const Vector3 centerOffset{ bounds.Center.x - light.position.x, bounds.Center.y - light.position.y, bounds.Center.z - light.position.z };
		const Vector3 extents{ bounds.Extents.x, bounds.Extents.y, bounds.Extents.z };
		const Vector3 nearestOffset{
			(std::max)(std::abs(centerOffset.x) - extents.x, 0.0f),
			(std::max)(std::abs(centerOffset.y) - extents.y, 0.0f),
			(std::max)(std::abs(centerOffset.z) - extents.z, 0.0f)
		};
		const float ratio = std::sqrt(nearestOffset.Dot(nearestOffset)) / (std::max)(light.range, 1e-4f);
		if (!(ratio < 1.0f)) { return 0.0f; }

		// RenderScene은 셰이더 기본 범위 창 감쇠를 사용한다.
		float attenuation = std::pow(1.0f - ratio * ratio, light.falloff);
		if (light.type == SpotLightType)
		{
			const float centerDistance = std::sqrt(centerOffset.Dot(centerOffset));
			const float radius = std::sqrt(extents.Dot(extents));
			// 경계 상자를 감싼 구에서 얻을 수 있는 최대 각도 감쇠로 추정한다.
			if (centerDistance > radius)
			{
				const Vector3 direction{ light.direction.x, light.direction.y, light.direction.z };
				const float centerCos = std::clamp(direction.Dot(centerOffset / centerDistance), -1.0f, 1.0f);
				const float closestAngle = (std::max)(0.0f, std::acos(centerCos) - std::asin(radius / centerDistance));
				const float cosOuter = std::cos(light.spotAngle * 0.5f);
				const float cosInner = light.spotInnerAngle > 0.0f ? std::cos((std::min)(light.spotInnerAngle, light.spotAngle) * 0.5f) : cosOuter + 0.05f;
				const float angularWeight = std::clamp((std::cos(closestAngle) - cosOuter) / (std::max)(cosInner - cosOuter, 1e-4f), 0.0f, 1.0f);
				attenuation *= angularWeight * angularWeight * (3.0f - 2.0f * angularWeight);
			}
		}

		return LightBrightness(light) * attenuation;
	}

	void FillAffectLights(std::span<const Hakko::DataHub::DataBox_RenderLight> lights, const DirectX::BoundingBox& bounds, std::uint32_t (&slots)[8])
	{
		struct RankedLight
		{
			std::uint32_t slot;
			float score;
		};

		std::array<RankedLight, RENDER_LIGHT_MAX - 1> ranked{};
		std::size_t count = 0;
		for (std::size_t index = 0; index < lights.size(); ++index)
		{
			const float score = EstimateLightContribution(lights[index], bounds);
			if (score > 0.0f)
			{
				ranked[count++] = { static_cast<std::uint32_t>(index + 1), score };
			}
		}

		const std::size_t selected = (std::min)(count, std::size(slots));
		std::partial_sort(ranked.begin(), ranked.begin() + selected, ranked.begin() + count, [](const RankedLight& lhs, const RankedLight& rhs)
		{
			if (lhs.score == rhs.score) { return lhs.slot < rhs.slot; }
			return lhs.score > rhs.score;
		});

		// 호출자가 0으로 초기화한 슬롯 배열에 영향을 주는 조명만 기록한다.
		for (std::size_t index = 0; index < selected; ++index)
		{
			slots[index] = ranked[index].slot;
		}
	}

	std::uint16_t CalculateTranslucentDepth(
		const Vector3& localCenter, const Matrix4x4& worldMatrix, const Matrix4x4& viewMatrix,
		float nearClipDistance, float farClipDistance)
	{
		// 행 벡터를 월드, 카메라 순으로 변환하고 정렬에 필요한 Z만 구한다.
		const Matrix4x4 worldView = worldMatrix * viewMatrix;
		const float viewZ = localCenter.x * worldView.m[0][2] + localCenter.y * worldView.m[1][2]
			+ localCenter.z * worldView.m[2][2] + worldView.m[3][2];
		const float normalizedDepth = (viewZ - nearClipDistance) / (farClipDistance - nearClipDistance);
		if (std::isnan(normalizedDepth)) { return 0; }

		// Hakko는 값이 큰 투명 요청부터 그린다.
		constexpr auto maxDepth = (std::numeric_limits<std::uint16_t>::max)();
		return static_cast<std::uint16_t>(std::clamp(normalizedDepth, 0.0f, 1.0f) * maxDepth);
	}

	/** 셰이더(HLSL)가 행렬을 열 단위로 읽으므로 전치하고, 렌더러가 받는 행렬 타입으로 옮긴다. */
	DirectX::XMMATRIX ToShaderMatrix(const Matrix4x4& matrix)
	{
		DirectX::XMFLOAT4X4 values;
		for (int row = 0; row < 4; ++row)
		{
			for (int col = 0; col < 4; ++col)
			{
				values.m[row][col] = matrix.m[col][row];
			}
		}

		return DirectX::XMLoadFloat4x4(&values);
	}

	/** 모든 뼈를 단위 행렬로 채운 뼈 행렬 묶음을 만든다. 뼈 자세를 계산하기 전에 메시를 원래 모양으로 그릴 때 쓴다. */
	Hakko::DataHub::DataBox_Render3DOptional CreateIdentityBoneData()
	{
		Hakko::DataHub::DataBox_Render3DOptional boneData{};
		for (DirectX::XMMATRIX& boneTransform : boneData.boneTransforms)
		{
			boneTransform = DirectX::XMMatrixIdentity();
		}
		return boneData;
	}
}

void RenderScene::AddStaticMeshComponent(StaticMeshComponent* component)
{
	if (component == nullptr) { return; }
	if (std::find(staticMeshComponents.begin(), staticMeshComponents.end(), component) != staticMeshComponents.end()) { return; }

	staticMeshComponents.push_back(component);
}

void RenderScene::RemoveStaticMeshComponent(StaticMeshComponent* component)
{
	const auto found = std::find(staticMeshComponents.begin(), staticMeshComponents.end(), component);
	if (found == staticMeshComponents.end()) { return; }

	for (std::unordered_map<const StaticMeshComponent*, Hakko::DataHub::DataBoxID>& objectDataIds : objectDataIdsByLayer)
	{
		const auto objectData = objectDataIds.find(component);
		if (objectData == objectDataIds.end()) { continue; }

		Hakko::DataHub::RemoveDataBox(Hakko::DataHub::DataBoxType_DtB_3DGeneral, objectData->second);
		objectDataIds.erase(objectData);
	}

	const auto boneData = boneDataIds.find(component);
	if (boneData != boneDataIds.end())
	{
		Hakko::DataHub::RemoveDataBox(Hakko::DataHub::DataBoxType_DtB_3DOptional, boneData->second);
		boneDataIds.erase(boneData);
	}

	// 그리는 순서가 의미를 가질 수 있으므로 마지막 원소와 바꾸지 않고 순서를 유지해 지운다.
	staticMeshComponents.erase(found);
}

void RenderScene::AddLightComponent(LightComponent* component)
{
	if (component == nullptr) { return; }
	if (std::find(lightComponents.begin(), lightComponents.end(), component) != lightComponents.end()) { return; }

	lightComponents.push_back(component);
}

void RenderScene::RemoveLightComponent(LightComponent* component)
{
	const auto found = std::find(lightComponents.begin(), lightComponents.end(), component);
	if (found == lightComponents.end()) { return; }

	lightComponents.erase(found);
}

void RenderScene::AddSkyboxComponent(SkyboxComponent* component)
{
	if (component == nullptr) { return; }
	if (std::find(skyboxComponents.begin(), skyboxComponents.end(), component) != skyboxComponents.end()) { return; }

	skyboxComponents.push_back(component);
}

void RenderScene::RemoveSkyboxComponent(SkyboxComponent* component)
{
	std::erase(skyboxComponents, component);
}

void RenderScene::PrepareBackground(std::uint8_t layer) const
{
	Hakko::DataHub::TexID skyboxTextureID = 0;
	for (const SkyboxComponent* component : skyboxComponents)
	{
		if (!component->IsEnabled()) { continue; }

		const std::array<std::shared_ptr<const Texture>, 6>& faces = component->GetFaces();
		std::array<const Texture*, 6> textureFaces{};
		for (std::size_t index = 0; index < faces.size(); ++index)
		{
			textureFaces[index] = faces[index].get();
		}
		if (std::find(textureFaces.begin(), textureFaces.end(), nullptr) != textureFaces.end()) { continue; }

		if (HakkoResources::PrepareSkyboxTexture(textureFaces, skyboxTextureID)) { break; }
	}

	if (skyboxTextureID == 0)
	{
		Hakko::DataHub::SetLayerBackground(RESERVED_TEX_NONE_DIFFUSE_ID, layer);
		Hakko::DataHub::SetLayerBackgroundColor(0.3f, 0.05f, 0.1f, 1.0f, layer);
		return;
	}

	Hakko::DataHub::SetLayerBackground(skyboxTextureID, layer);
	Hakko::DataHub::SetLayerBackgroundColor(1.0f, 1.0f, 1.0f, 1.0f, layer);
}

FramePreparationResult RenderScene::PrepareFrame(const CameraViewInfo& cameraViewInfo, const ViewportRect& viewport, float projectionAspectRatio, const Hakko::DataHub::ShadowAtlas& shadowAtlas, std::uint8_t layer)
{
	if (layer >= localLightsByLayer.size() || layer >= Hakko::DataHub::LayerList.size())
	{
		Log::Write(LogLevel::Error, "RenderScene", "Scene render layer is not registered");
		return FramePreparationResult::Failed;
	}

	ClearDrawRequests(layer);

	// 양수 비교로 0 이하인 크기와 NaN을 거른다.
	if (!(viewport.width > 0.0f) || !(viewport.height > 0.0f)) { return FramePreparationResult::SkipFrame; }
	if (!(projectionAspectRatio > 0.0f) || !std::isfinite(projectionAspectRatio)) { return FramePreparationResult::SkipFrame; }

	// 지정한 레이어의 그리기 동작이 이 영역만 사용하도록 한다.
	Hakko::DataHub::SetLayerViewport(Hakko::DataHub::Layer_Viewport{ viewport.x, viewport.y, viewport.width, viewport.height }, layer);

	const Matrix4x4 viewMatrix = cameraViewInfo.CalculateViewMatrix();
	if (!PrepareCamera(cameraViewInfo, viewMatrix, projectionAspectRatio, layer))
	{
		return FramePreparationResult::SkipFrame;
	}

	PrepareBackground(layer);
	const std::optional<Vector3> directionalLightDirection = PrepareGlobalLight(layer);

	if (!PrepareLocalLights(cameraViewInfo, viewMatrix, projectionAspectRatio, layer))
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to prepare local lights");
		return FramePreparationResult::Failed;
	}

	if (!PrepareStaticMeshes())
	{
		return FramePreparationResult::Failed;
	}

	const float splitEnd = (std::min)(DirectionalShadowDistanceCm, cameraViewInfo.farClipDistance);
	const bool hasDirectionalShadow = layer == 0U && directionalLightDirection.has_value() && splitEnd > cameraViewInfo.nearClipDistance;
	if (hasDirectionalShadow && !PrepareDirectionalShadow(cameraViewInfo, *directionalLightDirection, splitEnd, shadowAtlas))
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to prepare directional shadow cascade");
		return FramePreparationResult::Failed;
	}

	if (!PrepareDrawRequests(cameraViewInfo, viewMatrix, hasDirectionalShadow, layer))
	{
		return FramePreparationResult::Failed;
	}

	return FramePreparationResult::Ready;
}

bool RenderScene::PrepareStaticMeshes()
{
	for (const StaticMeshComponent* component : staticMeshComponents)
	{
		const std::shared_ptr<const StaticMesh>& mesh = component->GetStaticMesh();

		if (mesh == nullptr) { continue; }

		if (!HakkoResources::PrepareStaticMesh(*mesh)) { return false; }
	}

	return true;
}

bool RenderScene::PrepareCamera(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, float aspectRatio, std::uint8_t layer)
{
	Matrix4x4 projectionMatrix;
	if (!cameraViewInfo.CalculateProjectionMatrix(aspectRatio, projectionMatrix)) { return false; }

	const Matrix4x4 viewProjectionMatrix = viewMatrix * projectionMatrix;

	Hakko::DataHub::RTB_SetCamera camera{};
	camera.view = ToShaderMatrix(viewMatrix);
	camera.proj = ToShaderMatrix(projectionMatrix);
	camera.viewProj = ToShaderMatrix(viewProjectionMatrix);
	camera.eyePos = { cameraViewInfo.position.x, cameraViewInfo.position.y, cameraViewInfo.position.z };
	Hakko::DataHub::SendRTB(camera, layer);

	return true;
}

std::optional<Vector3> RenderScene::PrepareGlobalLight(std::uint8_t layer)
{
	Hakko::DataHub::RTB_SetGlobalLight light{};

	// 사용할 조명이 없어도 이전 프레임의 빛이 남지 않게 한다.
	light.ambient = { 0.0f, 0.0f, 0.0f, 1.0f };
	light.direction = { 0.0f, 0.0f, 1.0f };
	light.color = { 0.0f, 0.0f, 0.0f, 1.0f };

	std::optional<Vector3> directionalLightDirection;
	bool hasAmbientLight = false;
	for (const LightComponent* lightComponent : lightComponents)
	{
		if (!lightComponent->IsEnabled()) { continue; }

		if (!directionalLightDirection.has_value())
		{
			if (const DirectionalLightComponent* component = dynamic_cast<const DirectionalLightComponent*>(lightComponent))
			{
				Vector3 direction;
				if (component->GetLightDirection(direction))
				{
					const Vector3 color = component->GetColor();
					const float intensity = component->GetIntensity();
					light.direction = { direction.x, direction.y, direction.z };
					light.color = { color.x * intensity, color.y * intensity, color.z * intensity, 1.0f };
					directionalLightDirection = direction;
				}
			}
		}

		if (!hasAmbientLight)
		{
			if (const AmbientLightComponent* component = dynamic_cast<const AmbientLightComponent*>(lightComponent))
			{
				const Vector3 color = component->GetColor();
				const float intensity = component->GetIntensity();
				light.ambient = { color.x * intensity, color.y * intensity, color.z * intensity, 1.0f };
				hasAmbientLight = true;
			}
		}

		if (directionalLightDirection.has_value() && hasAmbientLight) { break; }
	}

	Hakko::DataHub::SendRTB(light, layer);
	return directionalLightDirection;
}

bool RenderScene::PrepareDirectionalShadow(const CameraViewInfo& cameraViewInfo, const Vector3& lightDirection, float splitEnd, const Hakko::DataHub::ShadowAtlas& shadowAtlas)
{
	constexpr float ShadowViewSizeCm = 8000.0f;
	constexpr float ShadowNearClipCm = 1.0f;
	constexpr float ShadowFarClipCm = 12000.0f;

	const Matrix4x4 cameraRotation = Matrix4x4::CreateFromQuaternion(cameraViewInfo.rotation);
	const Vector3 cameraForward{ cameraRotation.m[2][0], cameraRotation.m[2][1], cameraRotation.m[2][2] };
	// 화면 카메라의 그림자 깊이 구간 중간을 조명 직교 영역의 중심으로 삼는다.
	const float centerDepth = std::midpoint(cameraViewInfo.nearClipDistance, splitEnd);
	const Vector3 center = cameraViewInfo.position + cameraForward * centerDepth;

	const float lengthSquared = lightDirection.Dot(lightDirection);
	if (!(lengthSquared > 0.0f) || !std::isfinite(lengthSquared)) { return false; }

	const Vector3 lightForward = lightDirection / std::sqrt(lengthSquared);
	const Vector3 lightEye = center - lightForward * std::midpoint(ShadowNearClipCm, ShadowFarClipCm);
	// 빛이 수직에 가까울 때도 외적이 0에 가까워지지 않도록 기준 축을 바꾼다.
	const Vector3 referenceUp = std::abs(lightForward.y) < 0.99f ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 0.0f, 0.0f, 1.0f };
	const Vector3 right = referenceUp.Cross(lightForward);
	const Vector3 lightRight = right / std::sqrt(right.Dot(right));
	const Vector3 lightUp = lightForward.Cross(lightRight);

	// 월드 위치와 조명 카메라의 차이를 각 조명 축에 투영한다.
	const Matrix4x4 lightView{ {
		{ lightRight.x, lightUp.x, lightForward.x, 0.0f },
		{ lightRight.y, lightUp.y, lightForward.y, 0.0f },
		{ lightRight.z, lightUp.z, lightForward.z, 0.0f },
		{ -lightEye.Dot(lightRight), -lightEye.Dot(lightUp), -lightEye.Dot(lightForward), 1.0f }
	} };
	const Matrix4x4 lightProjection = Matrix4x4::CreateOrthographicViewSize(ShadowViewSizeCm, ShadowViewSizeCm, ShadowNearClipCm, ShadowFarClipCm);

	Hakko::DataHub::RTB_SetShadowCascade shadow{};
	shadow.atlas = shadowAtlas;
	shadow.lightViewProj = ToShaderMatrix(lightView * lightProjection);
	shadow.splitEnd = splitEnd;
	Hakko::DataHub::SendRTB(shadow);

	const Hakko::DataHub::RTB_ActionDraw& action = Hakko::DataHub::LayerList[0].actionGlobalSDWs[0];
	return action.output.useDS && action.actionDataBoxId != 0;
}

bool RenderScene::PrepareLocalLights(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, float aspectRatio, std::uint8_t layer)
{
	std::vector<Hakko::DataHub::DataBox_RenderLight> lights;
	for (const LightComponent* component : lightComponents)
	{
		if (!component->IsEnabled()) { continue; }

		Vector3 position;
		Hakko::DataHub::DataBox_RenderLight light{};
		if (const PointLightComponent* point = dynamic_cast<const PointLightComponent*>(component))
		{
			if (!point->GetLightPosition(position)) { continue; }
			light.type = PointLightType;
			light.range = point->GetRange();
		}
		else if (const SpotLightComponent* spot = dynamic_cast<const SpotLightComponent*>(component))
		{
			Vector3 direction;
			if (!spot->GetLightPose(position, direction)) { continue; }
			light.type = SpotLightType;
			light.range = spot->GetRange();
			light.direction = { direction.x, direction.y, direction.z };
			light.spotAngle = Math::DegreesToRadians(2.0f * spot->GetOuterConeAngleDegrees());
		}
		else
		{
			continue;
		}

		const Vector3 color = component->GetColor() * component->GetIntensity();
		light.position = { position.x, position.y, position.z };
		light.color = { color.x, color.y, color.z };
		lights.push_back(light);
	}
	SelectLocalLightsForView(lights, cameraViewInfo, viewMatrix, aspectRatio);

	RenderSceneLayerLightData& layerData = localLightsByLayer[layer];
	const Hakko::DataHub::RTB_SetLight emptySlot{};
	Hakko::DataHub::SendRTB(emptySlot, layer);
	for (std::size_t index = 0; index < layerData.ids.size(); ++index)
	{
		Hakko::DataHub::RTB_SetLight request{};
		request.slot = static_cast<std::uint16_t>(index + 1);
		if (index < lights.size())
		{
			Hakko::DataHub::DataBoxID& id = layerData.ids[index];
			if (id == 0)
			{
				id = Hakko::DataHub::AddDataBox(lights[index]);
				if (id == 0) { return false; }
			}
			else if (!Hakko::DataHub::UpdateDataBox(id, lights[index]))
			{
				return false;
			}

			request.light = id;
		}
		Hakko::DataHub::SendRTB(request, layer);
	}

	layerData.lights = std::move(lights);
	return true;
}

bool RenderScene::PrepareDrawRequests(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, bool hasDirectionalShadow, std::uint8_t layer)
{
	for (const StaticMeshComponent* component : staticMeshComponents)
	{
		const std::shared_ptr<const StaticMesh>& mesh = component->GetStaticMesh();
		if (mesh == nullptr) { continue; }

		const TransformComponent* transform = component->GetGameObject()->GetComponent<TransformComponent>();
		if (transform == nullptr) { continue; }

		const std::string& path = mesh->GetPath();
		const Hakko::DataHub::MeshID meshId = HakkoResources::FindStaticMeshId(*mesh);
		if (meshId == 0)
		{
			Log::Write(LogLevel::Error, "RenderScene", "Static mesh is not registered: " + path);
			ClearDrawRequests(layer);
			return false;
		}

		const Matrix4x4 worldMatrix = transform->GetWorldMatrix();

		Hakko::DataHub::DataBox_Render3DGeneral objectData{};
		objectData.worldMat = ToShaderMatrix(worldMatrix);
		// 재질 미지정 시에도 객체 데이터에는 유효한 수치를 채운다.
		// 정상 표면과 오류용 표면의 구분은 텍스처 조합에서 처리한다.
		objectData.blendColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		objectData.roughness = 0.5f;
		objectData.metalic = 0.0f;

		const std::shared_ptr<const Material>& material = component->GetMaterial();
		const bool isBlended = material != nullptr && material->alphaMode == MaterialAlphaMode::Blend;
		const bool usesObjectLights = isBlended || Hakko::DataHub::LayerList[layer].pipelineType == Hakko::DataHub::Layer_PipelineType_Forward;
		Vector3 localBoundsCenter;
		if (usesObjectLights)
		{
			const auto bounds = meshBounds.try_emplace(meshId);
			if (bounds.second)
			{
				bounds.first->second = CalculateLocalBounds(*mesh);
			}
			const DirectX::BoundingBox& localBounds = bounds.first->second;
			localBoundsCenter = { localBounds.Center.x, localBounds.Center.y, localBounds.Center.z };
			DirectX::BoundingBox worldBounds;
			localBounds.Transform(worldBounds, DirectX::XMMatrixTranspose(objectData.worldMat));
			FillAffectLights(localLightsByLayer[layer].lights, worldBounds, objectData.affectLights);
		}
		Hakko::DataHub::TexPropID texturePropID = 0;
		if (!HakkoResources::PrepareTextureProp(material.get(), texturePropID))
		{
			ClearDrawRequests(layer);
			return false;
		}

		if (material != nullptr)
		{
			const Vector3& color = material->baseColor;

			objectData.blendColor = { color.x, color.y, color.z, isBlended ? material->baseColorAlpha : 1.0f };
			objectData.roughness = material->roughness;
			objectData.metalic = material->metallic;
		}

		std::unordered_map<const StaticMeshComponent*, Hakko::DataHub::DataBoxID>& objectDataIds = objectDataIdsByLayer[layer];
		const auto found = objectDataIds.find(component);
		if (found == objectDataIds.end())
		{
			const Hakko::DataHub::DataBoxID dataId = Hakko::DataHub::AddDataBox(objectData);
			if (dataId == 0)
			{
				Log::Write(LogLevel::Error, "RenderScene", "Failed to register static mesh object data");
				ClearDrawRequests(layer);
				return false;
			}

			objectDataIds.emplace(component, dataId);
		}
		else if (!Hakko::DataHub::UpdateDataBox(found->second, objectData))
		{
			Log::Write(LogLevel::Error, "RenderScene", "Failed to update static mesh object data");
			ClearDrawRequests(layer);
			return false;
		}

		const Hakko::DataHub::DataBoxID objectDataId = objectDataIds.at(component);

		// HakkoResources::PrepareStaticMesh와 같은 기준이다. 스킨 정점으로 등록한 메시는 스킨용 설정과 뼈 행렬로 그린다.
		const bool isSkinned = !mesh->skinWeights.empty();

		// 0은 뼈 행렬 없음이다. 스킨 메시가 아니면 그대로 둔다.
		Hakko::DataHub::DataBoxID boneDataId = 0;
		if (isSkinned)
		{
			// 기본은 단위 행렬이라 SkinComponent가 없거나 계산할 수 없으면 메시를 원래 모양으로 그린다.
			Hakko::DataHub::DataBox_Render3DOptional boneData = CreateIdentityBoneData();

			const SkinComponent* skin = component->GetGameObject()->GetComponent<SkinComponent>();
			std::vector<Matrix4x4> boneMatrices;
			if (skin != nullptr && skin->CalculateBoneMatrices(boneMatrices))
			{
				// 셰이더의 뼈 행렬 배열보다 뼈가 많으면 정점이 배열 밖을 읽는다.
				if (boneMatrices.size() > SKIN_BONE_MAX)
				{
					Log::Write(LogLevel::Error, "RenderScene", "Skinned mesh has more joints than the renderer supports: " + path);
					ClearDrawRequests(layer);
					return false;
				}

				for (std::size_t i = 0; i < boneMatrices.size(); ++i)
				{
					boneData.boneTransforms[i] = ToShaderMatrix(boneMatrices[i]);
				}
			}

			// 객체 데이터처럼 처음에는 넣어 ID를 받아 두고, 이후 프레임에는 내용만 바꾼다.
			if (!boneDataIds.contains(component))
			{
				const Hakko::DataHub::DataBoxID newBoneDataId = Hakko::DataHub::AddDataBox(boneData);
				if (newBoneDataId == 0)
				{
					Log::Write(LogLevel::Error, "RenderScene", "Failed to register skinned mesh bone data");
					ClearDrawRequests(layer);
					return false;
				}

				boneDataIds.emplace(component, newBoneDataId);
			}
			else if (!Hakko::DataHub::UpdateDataBox(boneDataIds.at(component), boneData))
			{
				Log::Write(LogLevel::Error, "RenderScene", "Failed to update skinned mesh bone data");
				ClearDrawRequests(layer);
				return false;
			}

			boneDataId = boneDataIds.at(component);
		}

		if (isBlended)
		{
			Hakko::DataHub::RTB_DrawTP request{};
			request.drawProp = isSkinned ? RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID : RESERVED_DRAWPROP_TRANSLUCENT_ID;
			request.depth = CalculateTranslucentDepth(localBoundsCenter, worldMatrix, viewMatrix, cameraViewInfo.nearClipDistance, cameraViewInfo.farClipDistance);
			request.texProp = texturePropID;
			request.meshId = meshId;
			request.subMeshId = 0;
			request.generalId = objectDataId;
			request.optionalId = boneDataId;
			Hakko::DataHub::SendRTB(request, layer);
		}
		else
		{
			Hakko::DataHub::RTB_DrawOPQ request{};
			request.drawProp = isSkinned ? RESERVED_DRAWPROP_OPAQUE_SKINNED_ID : RESERVED_DRAWPROP_OPAQUE_ID;
			request.texProp = texturePropID;
			request.meshId = meshId;
			request.subMeshId = 0;
			request.generalId = objectDataId;
			request.optionalId = boneDataId;
			Hakko::DataHub::SendRTB(request, layer);

			if (hasDirectionalShadow)
			{
				const Hakko::DataHub::RTB_DrawSDW shadowRequest = Hakko::DataHub::MakeShadowRequest(request);
				Hakko::DataHub::SendRTB(shadowRequest, Hakko::DataHub::LayerList[layer].actionGlobalSDWs[0]);
			}
		}
	}

	return true;
}

void RenderScene::ReleaseLightData()
{
	for (std::size_t layer = 0; layer < localLightsByLayer.size(); ++layer)
	{
		RenderSceneLayerLightData& layerData = localLightsByLayer[layer];
		for (std::size_t index = 0; index < layerData.ids.size(); ++index)
		{
			Hakko::DataHub::DataBoxID& id = layerData.ids[index];
			if (id == 0) { continue; }

			const std::size_t slot = index + 1;
			// 다른 장면이 레이어를 다시 사용했으면 그 장면의 슬롯 연결은 유지한다.
			if (layer < Hakko::DataHub::LayerList.size() && Hakko::DataHub::LayerList[layer].lights[slot] == id)
			{
				Hakko::DataHub::RTB_SetLight request{};
				request.slot = static_cast<std::uint16_t>(slot);
				Hakko::DataHub::SendRTB(request, static_cast<std::uint8_t>(layer));
			}
			Hakko::DataHub::RemoveDataBox(Hakko::DataHub::DataBoxType_DtB_Light, id);
			id = 0;
		}
		layerData.lights.clear();
	}
}

void RenderScene::ClearObjectDataIds()
{
	for (std::unordered_map<const StaticMeshComponent*, Hakko::DataHub::DataBoxID>& objectDataIds : objectDataIdsByLayer)
	{
		objectDataIds.clear();
	}
	boneDataIds.clear();
	meshBounds.clear();
}
