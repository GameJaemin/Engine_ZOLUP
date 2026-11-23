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

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <string>

namespace
{
	// 드로우 데이터에서 사용하지 않는 조명 칸은 빈 슬롯 0을 참조한다.
	constexpr uint16_t pointLightSlot = 1;
	constexpr uint16_t spotLightSlot = 2;

	constexpr float DirectionalShadowDistanceCm = 5000.0f;

	void ClearDrawRequests()
	{
		Hakko::DataHub::ClearOpaqueDrawRequests();
		Hakko::DataHub::ClearTranslucentDrawRequests();
		Hakko::DataHub::ClearShadowDrawRequests();
	}

	Vector3 CalculateLocalBoundsCenter(const StaticMesh& mesh)
	{
		if (mesh.vertices.empty()) { return {}; }

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

		return {
			std::midpoint(minimum.x, maximum.x),
			std::midpoint(minimum.y, maximum.y),
			std::midpoint(minimum.z, maximum.z)
		};
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

	const auto objectData = objectDataIds.find(component);
	if (objectData != objectDataIds.end())
	{
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

void RenderScene::PrepareBackground() const
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
		Hakko::DataHub::SetLayerBackground(RESERVED_TEX_NONE_DIFFUSE_ID);
		Hakko::DataHub::SetLayerBackgroundColor(0.3f, 0.05f, 0.1f, 1.0f);
		return;
	}

	Hakko::DataHub::SetLayerBackground(skyboxTextureID);
	Hakko::DataHub::SetLayerBackgroundColor(1.0f, 1.0f, 1.0f, 1.0f);
}

FramePreparationResult RenderScene::PrepareFrame(const CameraViewInfo& cameraViewInfo, const ViewportRect& viewport, float projectionAspectRatio, const Hakko::DataHub::ShadowAtlas& shadowAtlas)
{
	ClearDrawRequests();

	// 양수 비교로 0 이하인 크기와 NaN을 거른다.
	if (!(viewport.width > 0.0f) || !(viewport.height > 0.0f)) { return FramePreparationResult::SkipFrame; }
	if (!(projectionAspectRatio > 0.0f) || !std::isfinite(projectionAspectRatio)) { return FramePreparationResult::SkipFrame; }

	// 기본 레이어의 그리기 동작이 이 영역만 사용하도록 한다.
	Hakko::DataHub::SetLayerViewport(Hakko::DataHub::Layer_Viewport{ viewport.x, viewport.y, viewport.width, viewport.height });

	const Matrix4x4 viewMatrix = cameraViewInfo.CalculateViewMatrix();
	if (!PrepareCamera(cameraViewInfo, viewMatrix, projectionAspectRatio))
	{
		return FramePreparationResult::SkipFrame;
	}

	PrepareBackground();
	const std::optional<Vector3> directionalLightDirection = PrepareGlobalLight();

	if (!PreparePointLight())
	{
		return FramePreparationResult::Failed;
	}

	if (!PrepareSpotLight())
	{
		return FramePreparationResult::Failed;
	}

	if (!PrepareStaticMeshes())
	{
		return FramePreparationResult::Failed;
	}

	const float splitEnd = (std::min)(DirectionalShadowDistanceCm, cameraViewInfo.farClipDistance);
	const bool hasDirectionalShadow = directionalLightDirection.has_value() && splitEnd > cameraViewInfo.nearClipDistance;
	if (hasDirectionalShadow && !PrepareDirectionalShadow(cameraViewInfo, *directionalLightDirection, splitEnd, shadowAtlas))
	{
		Log::Write(LogLevel::Error, "RenderScene", "Failed to prepare directional shadow cascade");
		return FramePreparationResult::Failed;
	}

	if (!PrepareDrawRequests(cameraViewInfo, viewMatrix, hasDirectionalShadow))
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

bool RenderScene::PrepareCamera(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, float aspectRatio)
{
	Matrix4x4 projectionMatrix;
	if (!cameraViewInfo.CalculateProjectionMatrix(aspectRatio, projectionMatrix)) { return false; }

	const Matrix4x4 viewProjectionMatrix = viewMatrix * projectionMatrix;

	Hakko::DataHub::RTB_SetCamera camera{};
	camera.view = ToShaderMatrix(viewMatrix);
	camera.proj = ToShaderMatrix(projectionMatrix);
	camera.viewProj = ToShaderMatrix(viewProjectionMatrix);
	camera.eyePos = { cameraViewInfo.position.x, cameraViewInfo.position.y, cameraViewInfo.position.z };
	Hakko::DataHub::SendRTB(camera);

	return true;
}

std::optional<Vector3> RenderScene::PrepareGlobalLight()
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
			if (const auto* component = dynamic_cast<const AmbientLightComponent*>(lightComponent))
			{
				const Vector3 color = component->GetColor();
				const float intensity = component->GetIntensity();
				light.ambient = { color.x * intensity, color.y * intensity, color.z * intensity, 1.0f };
				hasAmbientLight = true;
			}
		}

		if (directionalLightDirection.has_value() && hasAmbientLight) { break; }
	}

	Hakko::DataHub::SendRTB(light);
	return directionalLightDirection;
}

bool RenderScene::PrepareDirectionalShadow(const CameraViewInfo& cameraViewInfo, const Vector3& lightDirection, float splitEnd, const Hakko::DataHub::ShadowAtlas& shadowAtlas)
{
	constexpr float ShadowViewSizeCm = 8000.0f;
	constexpr float ShadowDepthCm = 12000.0f;
	constexpr float ShadowNearClipCm = 1.0f;

	const Matrix4x4 cameraRotation = Matrix4x4::CreateFromQuaternion(cameraViewInfo.rotation);
	const Vector3 cameraForward{ cameraRotation.m[2][0], cameraRotation.m[2][1], cameraRotation.m[2][2] };
	// 카메라의 그림자 구간 중간을 조명 직교 영역의 중심으로 잡는다.
	const float centerDepth = std::midpoint(cameraViewInfo.nearClipDistance, splitEnd);
	const Vector3 center = cameraViewInfo.position + cameraForward * centerDepth;
	const float lightLengthSquared = lightDirection.Dot(lightDirection);
	if (!(lightLengthSquared > 0.0f) || !std::isfinite(lightLengthSquared)) { return false; }

	const Vector3 lightForward = lightDirection / std::sqrt(lightLengthSquared);
	const Vector3 lightEye = center - lightForward * (ShadowDepthCm * 0.5f);
	// 수직에 가까운 빛에서도 뷰 행렬을 만들 수 있도록 다른 기준 축을 고른다.
	const Vector3 referenceUp = std::abs(lightForward.y) < 0.99f ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 0.0f, 0.0f, 1.0f };
	const Vector3 right = referenceUp.Cross(lightForward);
	const Vector3 lightRight = right / std::sqrt(right.Dot(right));
	const Vector3 lightUp = lightForward.Cross(lightRight);

	// 행 벡터를 각 열의 조명 축에 투영하고, 마지막 행에서 조명 카메라 위치를 뺀다.
	const Matrix4x4 lightView{ {
		{ lightRight.x, lightUp.x, lightForward.x, 0.0f },
		{ lightRight.y, lightUp.y, lightForward.y, 0.0f },
		{ lightRight.z, lightUp.z, lightForward.z, 0.0f },
		{ -lightEye.Dot(lightRight), -lightEye.Dot(lightUp), -lightEye.Dot(lightForward), 1.0f }
	} };
	const Matrix4x4 lightProjection = Matrix4x4::CreateOrthographicViewSize(ShadowViewSizeCm, ShadowViewSizeCm, ShadowNearClipCm, ShadowDepthCm);

	Hakko::DataHub::RTB_SetShadowCascade shadow{};
	shadow.atlas = shadowAtlas;
	shadow.lightViewProj = ToShaderMatrix(lightView * lightProjection);
	shadow.splitEnd = splitEnd;
	Hakko::DataHub::SendRTB(shadow);

	const Hakko::DataHub::RTB_ActionDraw& action = Hakko::DataHub::LayerList[0].actionGlobalSDWs[0];
	return action.output.useDS && action.actionDataBoxId != 0;
}

bool RenderScene::PreparePointLight()
{
	constexpr uint32_t pointLightType = 2;

	Hakko::DataHub::RTB_SetLight request{};
	request.slot = pointLightSlot;

	for (const LightComponent* lightComponent : lightComponents)
	{
		const auto* component = dynamic_cast<const PointLightComponent*>(lightComponent);
		if (component == nullptr || !component->IsEnabled()) { continue; }

		Vector3 position;
		if (!component->GetLightPosition(position)) { continue; }

		const Vector3 color = component->GetColor();
		const float intensity = component->GetIntensity();

		Hakko::DataHub::DataBox_RenderLight light{};
		light.type = pointLightType;
		light.position = { position.x, position.y, position.z };
		light.range = component->GetRange();
		light.color = { color.x * intensity, color.y * intensity, color.z * intensity };

		if (pointLightDataId == 0)
		{
			pointLightDataId = Hakko::DataHub::AddDataBox(light);
			if (pointLightDataId == 0) { return false; }
		}
		else if (!Hakko::DataHub::UpdateDataBox(pointLightDataId, light))
		{
			return false;
		}

		request.light = pointLightDataId;
		break;
	}

	Hakko::DataHub::SendRTB(request);
	return true;
}

bool RenderScene::PrepareSpotLight()
{
	constexpr uint32_t spotLightType = 3;

	Hakko::DataHub::RTB_SetLight request{};
	request.slot = spotLightSlot;

	for (const LightComponent* lightComponent : lightComponents)
	{
		const auto* component = dynamic_cast<const SpotLightComponent*>(lightComponent);
		if (component == nullptr || !component->IsEnabled()) { continue; }

		Vector3 position;
		Vector3 direction;
		if (!component->GetLightPose(position, direction)) { continue; }

		const Vector3 color = component->GetColor();
		const float intensity = component->GetIntensity();

		Hakko::DataHub::DataBox_RenderLight light{};
		light.type = spotLightType;
		light.position = { position.x, position.y, position.z };
		light.range = component->GetRange();
		light.color = { color.x * intensity, color.y * intensity, color.z * intensity };
		light.direction = { direction.x, direction.y, direction.z };
		light.spotAngle = Math::DegreesToRadians(2.0f * component->GetOuterConeAngleDegrees());

		if (spotLightDataId == 0)
		{
			spotLightDataId = Hakko::DataHub::AddDataBox(light);
			if (spotLightDataId == 0) { return false; }
		}
		else if (!Hakko::DataHub::UpdateDataBox(spotLightDataId, light))
		{
			return false;
		}

		request.light = spotLightDataId;
		break;
	}

	Hakko::DataHub::SendRTB(request);
	return true;
}

bool RenderScene::PrepareDrawRequests(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, bool hasDirectionalShadow)
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
			ClearDrawRequests();
			return false;
		}

		const Matrix4x4 worldMatrix = transform->GetWorldMatrix();

		Hakko::DataHub::DataBox_Render3DGeneral objectData{};
		objectData.affectLights[0] = pointLightSlot;
		objectData.affectLights[1] = spotLightSlot;
		objectData.worldMat = ToShaderMatrix(worldMatrix);
		// 재질 미지정 시에도 객체 데이터에는 유효한 수치를 채운다.
		// 정상 표면과 오류용 표면의 구분은 텍스처 조합에서 처리한다.
		objectData.blendColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		objectData.roughness = 0.5f;
		objectData.metalic = 0.0f;

		const std::shared_ptr<const Material>& material = component->GetMaterial();
		const bool isBlended = material != nullptr && material->alphaMode == MaterialAlphaMode::Blend;
		Hakko::DataHub::TexPropID texturePropID = 0;
		if (!HakkoResources::PrepareTextureProp(material.get(), texturePropID))
		{
			ClearDrawRequests();
			return false;
		}

		if (material != nullptr)
		{
			const Vector3& color = material->baseColor;

			objectData.blendColor = { color.x, color.y, color.z, isBlended ? material->baseColorAlpha : 1.0f };
			objectData.roughness = material->roughness;
			objectData.metalic = material->metallic;
		}

		const auto found = objectDataIds.find(component);
		if (found == objectDataIds.end())
		{
			const Hakko::DataHub::DataBoxID dataId = Hakko::DataHub::AddDataBox(objectData);
			if (dataId == 0)
			{
				Log::Write(LogLevel::Error, "RenderScene", "Failed to register static mesh object data");
				ClearDrawRequests();
				return false;
			}

			objectDataIds.emplace(component, dataId);
		}
		else if (!Hakko::DataHub::UpdateDataBox(found->second, objectData))
		{
			Log::Write(LogLevel::Error, "RenderScene", "Failed to update static mesh object data");
			ClearDrawRequests();
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
					ClearDrawRequests();
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
					ClearDrawRequests();
					return false;
				}

				boneDataIds.emplace(component, newBoneDataId);
			}
			else if (!Hakko::DataHub::UpdateDataBox(boneDataIds.at(component), boneData))
			{
				Log::Write(LogLevel::Error, "RenderScene", "Failed to update skinned mesh bone data");
				ClearDrawRequests();
				return false;
			}

			boneDataId = boneDataIds.at(component);
		}

		if (isBlended)
		{
			const auto [center, inserted] = translucentBoundsCenters.try_emplace(meshId);
			if (inserted) { center->second = CalculateLocalBoundsCenter(*mesh); }

			Hakko::DataHub::RTB_DrawTP request{};
			request.drawProp = isSkinned ? RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID : RESERVED_DRAWPROP_TRANSLUCENT_ID;
			request.depth = CalculateTranslucentDepth(
				center->second, worldMatrix, viewMatrix, cameraViewInfo.nearClipDistance, cameraViewInfo.farClipDistance);
			request.texProp = texturePropID;
			request.meshId = meshId;
			request.subMeshId = 0;
			request.generalId = objectDataId;
			request.optionalId = boneDataId;
			Hakko::DataHub::SendRTB(request);
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
			Hakko::DataHub::SendRTB(request);

			if (hasDirectionalShadow)
			{
				const Hakko::DataHub::RTB_DrawSDW shadowRequest = Hakko::DataHub::MakeShadowRequest(request);
				Hakko::DataHub::SendRTB(shadowRequest, Hakko::DataHub::LayerList[0].actionGlobalSDWs[0]);
			}
		}
	}

	return true;
}

void RenderScene::ReleaseLightData()
{
	if (pointLightDataId != 0)
	{
		Hakko::DataHub::RemoveDataBox(Hakko::DataHub::DataBoxType_DtB_Light, pointLightDataId);
		pointLightDataId = 0;
	}

	if (spotLightDataId != 0)
	{
		Hakko::DataHub::RemoveDataBox(Hakko::DataHub::DataBoxType_DtB_Light, spotLightDataId);
		spotLightDataId = 0;
	}
}

void RenderScene::ClearObjectDataIds()
{
	objectDataIds.clear();
	boneDataIds.clear();
	translucentBoundsCenters.clear();
}
