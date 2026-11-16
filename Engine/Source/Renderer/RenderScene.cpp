#include "RenderScene.h"

#include "CameraViewInfo.h"
#include "Component/AmbientLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/SpotLightComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "Core/Math/MathUtility.h"
#include "GameObject.h"
#include "HakkoResources.h"
#include "Mesh/StaticMesh.h"
#include "Renderer/Material.h"

#include "DataHub.h"
#include "DataHub_Interface.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

namespace
{
	// 드로우 데이터에서 사용하지 않는 조명 칸은 빈 슬롯 0을 참조한다.
	constexpr uint16_t pointLightSlot = 1;
	constexpr uint16_t spotLightSlot = 2;

	/** HLSL의 열 단위 저장에 맞춰 전치하고 Hakko가 받는 행렬 타입으로 옮긴다. */
	DirectX::XMMATRIX ToHakkoMatrix(const Matrix4x4& matrix)
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

FramePreparationResult RenderScene::PrepareFrame(const CameraViewInfo& cameraViewInfo, float aspectRatio)
{
	if (!PrepareCamera(cameraViewInfo, aspectRatio))
	{
		return FramePreparationResult::SkipFrame;
	}

	PrepareGlobalLight();

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

	if (!PrepareDrawRequests())
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

bool RenderScene::PrepareCamera(const CameraViewInfo& cameraViewInfo, float aspectRatio)
{
	Matrix4x4 projectionMatrix;
	if (!cameraViewInfo.CalculateProjectionMatrix(aspectRatio, projectionMatrix)) { return false; }

	const Matrix4x4 viewMatrix = cameraViewInfo.CalculateViewMatrix();
	const Matrix4x4 viewProjectionMatrix = viewMatrix * projectionMatrix;

	Hakko::DataHub::RTB_SetCamera camera{};
	camera.view = ToHakkoMatrix(viewMatrix);
	camera.proj = ToHakkoMatrix(projectionMatrix);
	camera.viewProj = ToHakkoMatrix(viewProjectionMatrix);
	camera.eyePos = { cameraViewInfo.position.x, cameraViewInfo.position.y, cameraViewInfo.position.z };
	Hakko::DataHub::SendRTB(camera);

	return true;
}

void RenderScene::PrepareGlobalLight()
{
	Hakko::DataHub::RTB_SetGlobalLight light{};

	// 사용할 조명이 없어도 이전 프레임의 빛이 남지 않게 한다.
	light.ambient = { 0.0f, 0.0f, 0.0f, 1.0f };
	light.direction = { 0.0f, 0.0f, 1.0f };
	light.color = { 0.0f, 0.0f, 0.0f, 1.0f };

	bool hasDirectionalLight = false;
	bool hasAmbientLight = false;
	for (const LightComponent* lightComponent : lightComponents)
	{
		if (!lightComponent->IsEnabled()) { continue; }

		if (!hasDirectionalLight)
		{
			if (const auto* component = dynamic_cast<const DirectionalLightComponent*>(lightComponent))
			{
				Vector3 direction;
				if (component->GetLightDirection(direction))
				{
					const Vector3 color = component->GetColor();
					const float intensity = component->GetIntensity();
					light.direction = { direction.x, direction.y, direction.z };
					light.color = { color.x * intensity, color.y * intensity, color.z * intensity, 1.0f };
					hasDirectionalLight = true;
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

		if (hasDirectionalLight && hasAmbientLight) { break; }
	}

	Hakko::DataHub::SendRTB(light);
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

bool RenderScene::PrepareDrawRequests()
{
	Hakko::DataHub::ClearOpaqueDrawRequests();

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
			Hakko::DataHub::ClearOpaqueDrawRequests();
			return false;
		}

		const Matrix4x4 worldMatrix = transform->GetWorldMatrix();

		Hakko::DataHub::DataBox_Render3DGeneral objectData{};
		objectData.affectLights[0] = pointLightSlot;
		objectData.affectLights[1] = spotLightSlot;
		objectData.worldMat = ToHakkoMatrix(worldMatrix);
		// 재질 미지정 시에도 객체 데이터에는 유효한 수치를 채운다.
		// 정상 표면과 오류용 표면의 구분은 텍스처 조합에서 처리한다.
		objectData.blendColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		objectData.roughness = 0.5f;
		objectData.metalic = 0.0f;

		const std::shared_ptr<const Material>& material = component->GetMaterial();
		Hakko::DataHub::TexPropID texturePropID = 0;
		if (!HakkoResources::PrepareTextureProp(material.get(), texturePropID))
		{
			Hakko::DataHub::ClearOpaqueDrawRequests();
			return false;
		}

		if (material != nullptr)
		{
			const Vector3& color = material->baseColor;

			objectData.blendColor = { color.x, color.y, color.z, 1.0f };
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
				Hakko::DataHub::ClearOpaqueDrawRequests();
				return false;
			}

			objectDataIds.emplace(component, dataId);
		}
		else if (!Hakko::DataHub::UpdateDataBox(found->second, objectData))
		{
			Log::Write(LogLevel::Error, "RenderScene", "Failed to update static mesh object data");
			Hakko::DataHub::ClearOpaqueDrawRequests();
			return false;
		}

		const Hakko::DataHub::DataBoxID objectDataId = objectDataIds.at(component);

		Hakko::DataHub::RTB_DrawOPQ request{};
		request.drawProp = RESERVED_DRAWPROP_OPAQUE_ID;
		request.texProp = texturePropID;
		request.meshId = meshId;
		request.subMeshId = 0;
		request.generalId = objectDataId;
		Hakko::DataHub::SendRTB(request);
	}

	return true;
}

void RenderScene::ClearObjectDataIds()
{
	objectDataIds.clear();
}
