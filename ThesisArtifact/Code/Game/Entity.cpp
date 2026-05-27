#include "Game/Entity.hpp"
#include "Engine/Math/Mat44.hpp"

Entity::Entity(Game* owner)
	:m_game(owner)
{
}

Entity::Entity(Game* owner, Vec3 const& position, EulerAngles const& orientation, float uniformScale)
	:m_game(owner)
	,m_position(position)
	,m_orientationEuler(orientation)
	,m_uniformScale(uniformScale)
{
}

Entity::~Entity()
{
}

Mat44 Entity::GetModelToWorldTransform() const
{
	Mat44 transform = Mat44::MakeTranslation3D(m_position);
	Mat44 rotationMat = m_orientationEuler.GetAsMatrix_IFwd_JLeft_KUp();
	transform.Append(rotationMat);
	transform.AppendScaleUniform3D(m_uniformScale);
	return transform;
}

Vec3 Entity::GetForwardNormal() const
{
	return m_orientationEuler.Get_IFwd();
}
