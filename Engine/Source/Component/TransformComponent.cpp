#include "TransformComponent.h"

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"

Transform TransformComponent::GetTransform() const
{
	return transform;
}

void TransformComponent::SetTransform(const Transform& newTransform)
{
	transform = newTransform;
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

Vector3 TransformComponent::GetForwardVector() const
{
	const Matrix4x4 rotationMatrix = Matrix4x4::CreateFromQuaternion(transform.rotation);

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
