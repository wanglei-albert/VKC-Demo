#ifndef VKC_USE_ACTION_H
#define VKC_USE_ACTION_H

#include <vkc/action/action_base.h>


namespace vkc
{

class UseAction: public ActionBase
{
public:
  using Ptr = std::shared_ptr<UseAction>;

  UseAction(
    std::string manipulator_id, 
    std::string attached_object_id,
    Eigen::Isometry3d tf,
    std::string end_effector_id=""
  ):
    ActionBase(ActionType::UseAction, manipulator_id), 
    attached_object_id_(attached_object_id),
    tf_(tf),
    end_effector_id_(end_effector_id)
  {}

  ~UseAction() = default;

  std::string getAttachedObject()
  {
    return attached_object_id_;
  }

  Eigen::Isometry3d& getTransform()
  {
    return tf_;
  }

  std::string getEndEffectorID()
  {
    return end_effector_id_;
  }

private:
  std::string attached_object_id_;
  Eigen::Isometry3d tf_;
  std::string end_effector_id_;
};

} // end of namespace vkc

#endif