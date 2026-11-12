#include "RenderScene.h"

#include "CameraViewInfo.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "GameObject.h"
#include "Mesh/StaticMesh.h"
#include "Renderer/Material.h"

#include "DataHub_DataBoxCustomizer.h"
#include "DataHub_RenderIdProvider.h"
#include "DataHub_Resource.h"
#include "DataHub_RTB.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

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
		Hakko::DataHub::DataBoxCustomizer::RemoveDataBox<Hakko::DataHub::DataBox_Render3DGeneral>(objectData->second);
		objectDataIds.erase(objectData);
	}

	// 그리는 순서가 의미를 가질 수 있으므로 마지막 원소와 바꾸지 않고 순서를 유지해 지운다.
	staticMeshComponents.erase(found);
}

void RenderScene::AddDirectionalLightComponent(DirectionalLightComponent* component)
{
	if (component == nullptr) { return; }
	if (std::find(directionalLightComponents.begin(), directionalLightComponents.end(), component) != directionalLightComponents.end()) { return; }

	directionalLightComponents.push_back(component);
}

void RenderScene::RemoveDirectionalLightComponent(DirectionalLightComponent* component)
{
	const auto found = std::find(directionalLightComponents.begin(), directionalLightComponents.end(), component);
	if (found == directionalLightComponents.end()) { return; }

	directionalLightComponents.erase(found);
}

bool RenderScene::PrepareStaticMeshes()
{
	for (const StaticMeshComponent* component : staticMeshComponents)
	{
		const std::shared_ptr<const StaticMesh>& mesh = component->GetStaticMesh();

		if (mesh == nullptr) { continue; }

		const std::string& path = mesh->GetPath();
		if (path.empty())
		{
			Log::Write(LogLevel::Error, "RenderScene", "Static mesh registration requires an asset path");
			return false;
		}

		// Hakko는 wchar_t 이름을 받으므로 UTF-8 경로를 변환한다.
		const std::wstring name = std::filesystem::path(std::u8string(path.begin(), path.end())).wstring();
		if (Hakko::DataHub::GetMeshId(name) != 0) { continue; }

		Hakko::DataHub::Mesh renderMesh{};
		renderMesh.vertexType = Hakko::DataHub::VertexType_Static;
		renderMesh.verticesStatic.reserve(mesh->vertices.size());

		for (const MeshVertex& vertex : mesh->vertices)
		{
			Hakko::DataHub::VertexStatic renderVertex{};
			renderVertex.Pos = { vertex.position.x, vertex.position.y, vertex.position.z };
			renderVertex.UV = { vertex.uv.x, vertex.uv.y };
			renderVertex.Normal = { vertex.normal.x, vertex.normal.y, vertex.normal.z };
			renderVertex.Tangent = { vertex.tangent.x, vertex.tangent.y, vertex.tangent.z };
			// 엔진 정점에는 색상이 없으므로 흰색으로 채운다.
			renderVertex.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

			renderMesh.verticesStatic.push_back(renderVertex);
		}

		// Hakko의 인덱스 타입이 std::uint32_t로 전환된 것을 전제로 한다.
		renderMesh.indices = mesh->indices;

		const Hakko::DataHub::MeshID meshId = Hakko::DataHub::CreateMesh(name.c_str(), std::move(renderMesh));

		if (meshId == 0)
		{
			Log::Write(LogLevel::Error, "RenderScene", "Failed to register static mesh: " + path);

			return false;
		}

		Hakko::DataHub::flag_updateResources_Upload = true;
	}

	return true;
}

bool RenderScene::PrepareCamera(const CameraViewInfo& cameraViewInfo, float aspectRatio)
{
	Matrix4x4 projectionMatrix;
	if (!cameraViewInfo.CalculateProjectionMatrix(aspectRatio, projectionMatrix)) { return false; }

	const Matrix4x4 viewMatrix = cameraViewInfo.CalculateViewMatrix();
	const Matrix4x4 viewProjectionMatrix = viewMatrix * projectionMatrix;

	const auto toShaderMatrix = [](const Matrix4x4& matrix)
	{
		DirectX::XMFLOAT4X4 values;
		std::memcpy(&values, matrix.m, sizeof(values));

		// Hakko는 그대로 복사하므로 HLSL 기본 열 단위 저장에 맞춰 전치한다.
		return DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&values));
	};

	auto& condition = Hakko::DataHub::LayerList[0].condition;
	condition.camera_View = toShaderMatrix(viewMatrix);
	condition.camera_Proj = toShaderMatrix(projectionMatrix);
	condition.camera_ViewProj = toShaderMatrix(viewProjectionMatrix);
	condition.camera_EyePos = {
		cameraViewInfo.position.x,
		cameraViewInfo.position.y,
		cameraViewInfo.position.z
	};

	return true;
}

void RenderScene::PrepareDirectionalLight()
{
	auto& condition = Hakko::DataHub::LayerList[0].condition;

	// 사용할 조명이 없어도 이전 프레임의 빛이 남지 않게 한다.
	condition.global_LightDirection = { 0.0f, 0.0f, 1.0f };
	condition.global_LightColor = { 0.0f, 0.0f, 0.0f, 1.0f };

	for (const DirectionalLightComponent* component : directionalLightComponents)
	{
		if (!component->IsEnabled()) { continue; }

		Vector3 direction;
		if (!component->GetLightDirection(direction)) { continue; }

		const Vector3 color = component->GetColor();
		const float intensity = component->GetIntensity();

		condition.global_LightDirection = { direction.x, direction.y, direction.z };

		condition.global_LightColor = { color.x * intensity, color.y * intensity, color.z * intensity, 1.0f };

		return;
	}
}

bool RenderScene::PrepareDrawRequests()
{
	auto& requests = Hakko::DataHub::LayerList[0].actionOPQ.drawReqList;
	requests.clear();

	for (const StaticMeshComponent* component : staticMeshComponents)
	{
		const std::shared_ptr<const StaticMesh>& mesh = component->GetStaticMesh();
		if (mesh == nullptr) { continue; }

		const TransformComponent* transform = component->GetGameObject()->GetComponent<TransformComponent>();
		if (transform == nullptr) { continue; }

		const std::string& path = mesh->GetPath();
		const std::wstring name = std::filesystem::path(std::u8string(path.begin(), path.end())).wstring();
		const Hakko::DataHub::MeshID meshId = Hakko::DataHub::GetMeshId(name);
		if (meshId == 0)
		{
			Log::Write(LogLevel::Error, "RenderScene", "Static mesh is not registered: " + path);
			requests.clear();
			return false;
		}

		const Matrix4x4 worldMatrix = transform->GetTransform().ToMatrix();
		DirectX::XMFLOAT4X4 values;
		std::memcpy(&values, worldMatrix.m, sizeof(values));

		Hakko::DataHub::DataBox_Render3DGeneral objectData{};
		// 카메라와 동일하게 HLSL의 열 단위 저장에 맞춰 전치한다.
		objectData.worldMat = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&values));
		// 재질 미지정 시에도 객체 데이터에는 유효한 수치를 채운다.
		// 정상 표면과 오류용 표면의 구분은 텍스처 조합에서 처리한다.
		objectData.blendColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		objectData.roughness = 0.5f;
		objectData.metalic = 0.0f;

		const std::shared_ptr<const Material>& material = component->GetMaterial();
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
			const Hakko::DataHub::DataBoxID dataId = Hakko::DataHub::DataBoxCustomizer::AddDataBox(std::move(objectData));
			if (dataId == 0)
			{
				Log::Write(LogLevel::Error, "RenderScene", "Failed to register static mesh object data");
				return false;
			}

			objectDataIds.emplace(component, dataId);
		}
		else
		{
			auto& dataBoxes = Hakko::DataHub::DataBoxCustomizer::GetDataBoxList<Hakko::DataHub::DataBox_Render3DGeneral>();
			dataBoxes[found->second] = objectData;
		}

		const Hakko::DataHub::DataBoxID objectDataId = objectDataIds.at(component);

		Hakko::DataHub::RenderIdOpaque renderId;
		renderId.drawProp = RESERVED_DRAWPROP_OPAQUE_ID;
		renderId.texProp = 0; // 임시 대체 조합. 정상적인 기본 표면 연결은 후속 단계다.
		renderId.meshId = meshId;

		Hakko::DataHub::RTB_DrawRequest request{};
		request.renderId = renderId;
		request.meshId = meshId;
		request.subMeshId = 0;
		request.dataBoxIds[0] = objectDataId;

		requests.push_back(request);
	}

	return true;
}

void RenderScene::ClearObjectDataIds()
{
	objectDataIds.clear();
}
