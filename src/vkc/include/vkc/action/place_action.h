#ifndef VKC_PLACE_ACTION_H
#define VKC_PLACE_ACTION_H

#include <vkc/action/action_base.h>


namespace vkc
{

class PlaceAction: public ActionBase
{
public:
  using Ptr = std::shared_ptr<PlaceAction>;

  PlaceAction(
    std::string manipulator_id, 
    std::string detached_object_id,
    std::vector<LinkDesiredPose> link_objectives,
    std::vector<JointDesiredPose> joint_objectives
    // LinkDesiredPose base_objective
  ):
    ActionBase(ActionType::PlaceAction, manipulator_id), 
    detached_object_id_(detached_object_id),
    link_objectives_(link_objectives),
    joint_objectives_(joint_objectives)
    // base_objective_(base_objective)
  {}

  ~PlaceAction() = default;

  std::string getDetachedObject()
  {
    return detached_object_id_;
  }

  std::vector<LinkDesiredPose>& getLinkObjectives()
  {
    return link_objectives_;
  }

  void addLinkObjectives(LinkDesiredPose link_pose)
  {
    link_objectives_.push_back(link_pose);
    return;
  }

  // LinkDesiredPose& getBaseObjective()
  // {
  //   return base_objective_;
  // }

  std::vector<JointDesiredPose>& getJointObjectives()
  {
    return joint_objectives_;
  }

private:
  std::string detached_object_id_;
  std::vector<LinkDesiredPose> link_objectives_;
  std::vector<JointDesiredPose> joint_objectives_;
  // LinkDesiredPose base_objective_;
};

} // end of namespace vkc

#endif