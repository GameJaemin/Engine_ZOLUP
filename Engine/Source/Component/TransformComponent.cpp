#include "TransformComponent.h"

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"
#include "GameObject.h"

#include <cmath>

Transform TransformComponent::GetTransform() const
{
	return transform;
}

void TransformComponent::SetTransform(const Transform& newTransform)
{
	transform = newTransform;
}

Matrix4x4 TransformComponent::GetWorldMatrix() const
{
	// 자기 행렬에 부모, 조부모의 행렬을 차례로 오른쪽에 곱하면 월드 행렬이 된다.
	Matrix4x4 worldMatrix = transform.ToMatrix();

	// 부착 전이라 소속 객체가 없으면 부모도 없다.
	const GameObject* owner = GetGameObject();
	if (owner == nullptr) { return worldMatrix; }

	const GameObject* parent = owner->GetParent();
	while (parent != nullptr)
	{
		// 모든 GameObject는 생성자에서 TransformComponent를 붙인다.
		const TransformComponent* parentTransform = parent->GetComponent<TransformComponent>();
		worldMatrix = worldMatrix * parentTransform->GetTransform().ToMatrix();

		// 한 단계 위로 올라간다.
		parent = parent->GetParent();
	}

	return worldMatrix;
}

Transform TransformComponent::GetWorldTransform() const
{
	return Transform::CreateFromMatrix(GetWorldMatrix());
}

Vector3 TransformComponent::GetPosition() const
{
	return transform.position;
}

void TransformComponent::SetPosition(const Vector3& newPosition)
{
	transform.position = newPosition;
}

EulerAngles TransformComponent::GetRotation() const
{
	return transform.rotation.ToEulerAngles();
}

void TransformComponent::SetRotation(const EulerAngles& newRotation)
{
	transform.rotation = Quaternion::CreateFromEulerAngles(newRotation);
}

Quaternion TransformComponent::GetRotationQuaternion() const
{
	return transform.rotation;
}

bool TransformComponent::SetRotationQuaternion(const Quaternion& newRotation)
{
	if (!std::isfinite(newRotation.x) || !std::isfinite(newRotation.y) || !std::isfinite(newRotation.z) || !std::isfinite(newRotation.w)) { return false; }

	// 길이가 1에서 이보다 더 벗어나면 단위 쿼터니언이 아니라고 보고 거부한다. glTF 로더가 받는 허용치(0.001)와 같게 해 불러온 회전을 거부하지 않는다.
	// 정규화하면 값이 바뀌어 저장한 회전을 그대로 되돌릴 수 없으므로 하지 않는다.
	constexpr float UnitLengthTolerance = 0.001f;
	const float lengthSquared = newRotation.x * newRotation.x + newRotation.y * newRotation.y + newRotation.z * newRotation.z + newRotation.w * newRotation.w;
	if (std::fabs(std::sqrt(lengthSquared) - 1.0f) > UnitLengthTolerance) { return false; }

	transform.rotation = newRotation;
	return true;
}

Vector3 TransformComponent::GetForwardVector() const
{
	const Matrix4x4 rotationMatrix = Matrix4x4::CreateFromQuaternion(GetWorldTransform().rotation);

	// 행 벡터 (0, 0, 1)과 회전 행렬을 곱한 결과는 세 번째 행이다.
	return { rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2] };
}

Vector3 TransformComponent::GetScale() const
{
	return transform.scale;
}

void TransformComponent::SetScale(const Vector3& newScale)
{
	transform.scale = newScale;
}
