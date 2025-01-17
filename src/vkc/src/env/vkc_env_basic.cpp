#include <vkc/env/vkc_env_basic.h>

const std::string DEFAULT_VKC_GROUP_ID = "vkc";

namespace vkc
{
// namespace vkc starts

VKCEnvBasic::VKCEnvBasic(ros::NodeHandle nh, bool plotting, bool rviz)
  : nh_(nh), plotting_(plotting), rviz_(rviz), tesseract_(std::make_shared<vkc::ConstructVKC>())
{
  // Initial number of past revisions
  n_past_revisions_ = 0;
}

void VKCEnvBasic::setEndEffector(std::string link_name)
{
  end_effector_link_ = link_name;
}

void VKCEnvBasic::setRobotEndEffector(std::string link_name)
{
  robot_end_effector_link_ = link_name;
}

ConstructVKC::Ptr VKCEnvBasic::getVKCEnv()
{
  return tesseract_;
}

bool VKCEnvBasic::loadRobotModel(const std::string& ENV_DESCRIPTION_PARAM, const std::string& ENV_SEMANTIC_PARAM,
                                 const std::string& END_EFFECTOR_LINK)
{
  // Initial setup, load xml directory defined in launch
  std::string env_urdf_xml_string, env_srdf_xml_string, end_effector_link;

  nh_.getParam(ENV_DESCRIPTION_PARAM, env_urdf_xml_string);
  nh_.getParam(ENV_SEMANTIC_PARAM, env_srdf_xml_string);
  nh_.getParam(END_EFFECTOR_LINK, end_effector_link);

  // at the beginning stage, current end effector
  // is the robot end effector
  setEndEffector(end_effector_link);
  setRobotEndEffector(end_effector_link);

  ROS_INFO("Loading environment URDF to scene graph...");
  ResourceLocator::Ptr locator = std::make_shared<tesseract_rosutils::ROSResourceLocator>();

  if (!tesseract_->loadURDFtoSceneGraph(env_urdf_xml_string, env_srdf_xml_string, locator))
  {
    ROS_INFO("Failed to generate environment scene graph");
    return false;
  }

  return true;
}

bool VKCEnvBasic::initTesseractConfig(const std::string& modify_env_srv, const std::string& get_env_changes_srv)
{
  // Initialize scene graph to tesseract environment
  ROS_INFO("Initializing tesseract...");

  tesseract_->initTesseract();

  ROS_INFO("Tesseract initialized...");

  // These are used to keep visualization updated
  if (rviz_)
  {
    modify_env_rviz_ = nh_.serviceClient<tesseract_msgs::ModifyEnvironment>(modify_env_srv, false);
    get_env_changes_rviz_ = nh_.serviceClient<tesseract_msgs::GetEnvironmentChanges>(get_env_changes_srv, false);

    // Check RViz to make sure nothing has changed
    if (!checkRviz())
      return false;
  }

  return true;
}

std::unordered_map<std::string, double> VKCEnvBasic::getHomePose()
{
  return home_pose_;
}

void VKCEnvBasic::addAttachedLink(std::string link_name)
{
  attached_links_.push_back(link_name);
}

void VKCEnvBasic::removeTopAttachedLink()
{
  attached_links_.pop_back();
}

std::string VKCEnvBasic::getTopAttachedLink()
{
  return attached_links_.back();
}

bool VKCEnvBasic::isRobotArmFree()
{
  return attached_links_.size() == 0;
}

bool VKCEnvBasic::ifAttachedLink(std::string link_name)
{
  // check if link_name already in the attched_links
  return (find(attached_links_.begin(), attached_links_.end(), link_name) != attached_links_.end());
}

void VKCEnvBasic::addAttachLocation(vkc::BaseObject::AttachLocation attach_location)
{
  vkc::BaseObject::AttachLocation::Ptr al_ptr = std::make_shared<vkc::BaseObject::AttachLocation>(std::move(attach_location));
  al_ptr->connection.parent_link_name = end_effector_link_;
  attach_locations_[al_ptr->name_] = al_ptr;
}

void VKCEnvBasic::addAttachLocations(
    std::unordered_map<std::string, vkc::BaseObject::AttachLocation::Ptr> attach_locations)
{
  for (auto& attach_location : attach_locations)
  {
    attach_locations_[attach_location.first] = attach_location.second;
    attach_locations_[attach_location.first]->connection.parent_link_name = end_effector_link_;
  }
}

std::string VKCEnvBasic::getEndEffectorLink(){
  return end_effector_link_;
}

vkc::BaseObject::AttachLocation::Ptr VKCEnvBasic::getAttachLocation(std::string link_name)
{
  auto attach_location = attach_locations_.find(link_name);
  // cannot find the attached link
  if (attach_location == attach_locations_.end())
    return nullptr;

  return attach_location->second;
}

bool VKCEnvBasic::setHomePose()
{
  for (const auto& group_state : tesseract_->getTesseract()->getSRDFModel()->getGroupStates())
  {
    if (group_state.name_ != "home")
    {
      continue;
    }
    for (auto const& val : group_state.joint_values_)
    {
      // TODO: support for joints with multi-dofs
      home_pose_[val.first] = val.second[0];
    }
  }
  if (home_pose_.size() <= 0)
  {
    ROS_WARN("No home pose defined!");
    return false;
  }
  tesseract_->getTesseract()->getEnvironment()->setState(home_pose_);
  return true;
}

bool VKCEnvBasic::setInitPose(std::unordered_map<std::string, double> init_pose)
{
  tesseract_->getTesseract()->getEnvironment()->setState(init_pose);
  return true;
}

bool VKCEnvBasic::checkRviz()
{
  // Get the current state of the environment.
  // Usually you would not be getting environment state from rviz
  // this is just an example. You would be gettting it from the
  // environment_monitor node. Need to update examples to launch
  // environment_monitor node.
  get_env_changes_rviz_.waitForExistence();
  tesseract_msgs::GetEnvironmentChanges env_changes;
  env_changes.request.revision = 0;
  if (get_env_changes_rviz_.call(env_changes))
  {
    ROS_INFO("Retrieve current environment changes!");
  }
  else
  {
    ROS_ERROR("Failed to retrieve current environment changes!");
    return false;
  }

  // There should not be any changes but check
  if (env_changes.response.revision != 0)
  {
    ROS_ERROR("The environment has changed externally!");
    return false;
  }
  return true;
}

bool VKCEnvBasic::sendRvizChanges()
{
  bool ret = sendRvizChanges_(n_past_revisions_);

  n_past_revisions_ = tesseract_->getTesseract()->getEnvironment()->getRevision();

  return ret;
}

bool VKCEnvBasic::sendRvizChanges_(unsigned long past_revision)
{
  modify_env_rviz_.waitForExistence();
  tesseract_msgs::ModifyEnvironment update_env;
  update_env.request.id = tesseract_->getTesseract()->getEnvironment()->getName();
  update_env.request.revision = past_revision;
  if (!tesseract_rosutils::toMsg(update_env.request.commands,
                                 tesseract_->getTesseract()->getEnvironment()->getCommandHistory(),
                                 update_env.request.revision))
  {
    ROS_ERROR("Failed to generate commands to update rviz environment!");
    return false;
  }

  if (modify_env_rviz_.call(update_env))
  {
    ROS_INFO("RViz environment Updated!");
  }
  else
  {
    ROS_INFO("Failed to update rviz environment");
    return false;
  }

  return true;
}

void VKCEnvBasic::updateAttachLocParentLink(std::string attach_loc_name, std::string parent_link_name)
{
  if (ifAttachedLink(attach_loc_name))
  {
    ROS_ERROR("Cannot change parent link name since ", attach_loc_name.c_str(), " is already attached to another link");
  }
  attach_locations_.at(attach_loc_name)->connection.parent_link_name = parent_link_name;
}

void VKCEnvBasic::attachObject(std::string attach_location_name, Eigen::Isometry3d* tf)
{ 
  // update current end effector to the parent link of the attached object
  updateAttachLocParentLink(attach_location_name, end_effector_link_);

  // specified transform between end effector and attached location
  if (tf != nullptr)
  {
    // attach_locations_.at(attach_location_name)->connection.parent_to_joint_origin_transform = (*tf).inverse();
    attach_locations_.at(attach_location_name)->connection.parent_to_joint_origin_transform = tesseract_->getTesseract()->getEnvironment()->getLinkTransform(getEndEffectorLink()).inverse() *
    tesseract_->getTesseract()->getEnvironment()->getLinkTransform(attach_locations_.at(attach_location_name)->connection.child_link_name);
    attach_locations_.at(attach_location_name)->connection.parent_link_name = getEndEffectorLink();
  }
  // default transform
  else
  {
    attach_locations_.at(attach_location_name)->connection.parent_to_joint_origin_transform = tesseract_->getTesseract()->getEnvironment()->getLinkTransform(getEndEffectorLink()).inverse() *
    tesseract_->getTesseract()->getEnvironment()->getLinkTransform(attach_locations_.at(attach_location_name)->connection.child_link_name);
        // attach_locations_.at(attach_location_name)->local_joint_origin_transform.inverse();
    attach_locations_.at(attach_location_name)->connection.parent_link_name = getEndEffectorLink();
  }
  std::cout << getEndEffectorLink() << std::endl;

  // std::cout << tesseract_->getTesseract()->getEnvironment()->getLinkTransform(getEndEffectorLink()).translation() << std::endl;
  // std::cout << tesseract_->getTesseract()->getEnvironment()->getLinkTransform(getEndEffectorLink()).linear() << std::endl;
  // std::cout << tesseract_->getTesseract()->getEnvironment()->getLinkTransform(attach_locations_.at(attach_location_name)->connection.child_link_name).translation() << std::endl;
  // std::cout << tesseract_->getTesseract()->getEnvironment()->getLinkTransform(attach_locations_.at(attach_location_name)->connection.child_link_name).linear() << std::endl;
  // std::cout << attach_locations_.at(attach_location_name)->connection.parent_link_name << std::endl;
  // std::cout << attach_locations_.at(attach_location_name)->connection.child_link_name << std::endl;

  tesseract_->getTesseract()->getEnvironment()->moveLink((attach_locations_.at(attach_location_name)->connection));

  end_effector_link_ = attach_locations_.at(attach_location_name)->base_link_;
  addAttachedLink(attach_location_name);
}

void VKCEnvBasic::detachTopObject()
{
  std::string target_location_name = getTopAttachedLink();
  removeTopAttachedLink();

  end_effector_link_ = attach_locations_.at(target_location_name)->connection.parent_link_name;

  std::string link_name = attach_locations_.at(target_location_name)->link_name_;
  std::string object_name = link_name.substr(0, link_name.find("_", 0));
  Joint world_joint(object_name + "_world");
  world_joint.parent_link_name = "world";
  world_joint.child_link_name = link_name;
  world_joint.type = JointType::FIXED;
  world_joint.parent_to_joint_origin_transform = Eigen::Isometry3d::Identity();
  world_joint.parent_to_joint_origin_transform =
      tesseract_->getTesseract()->getEnvironment()->getLinkTransform(link_name);
  tesseract_->getTesseract()->getEnvironment()->moveLink(world_joint);
  // std::cout << tesseract_->getTesseract()->getEnvironment()->getLinkTransform(link_name).translation() << std::endl;
  attach_locations_.at(target_location_name)->world_joint_origin_transform =
      tesseract_->getTesseract()->getEnvironment()->getLinkTransform(link_name) *
      attach_locations_.at(target_location_name)->local_joint_origin_transform;
}

void VKCEnvBasic::detachObject(std::string detach_location_name)
{
  if (!ifAttachedLink(detach_location_name))
    return;

  // detach previously attached links
  while (getTopAttachedLink() != detach_location_name)
    detachTopObject();

  // detach the real target detach_location_name
  detachTopObject();
}

std::string VKCEnvBasic::updateEnv(std::vector<std::string>& joint_names,
                                   tesseract_motion_planners::PlannerResponse& response, ActionBase::Ptr action)
{
  std::cout << "VKCEnvBasic::updateEnv" << std::endl;
  std::string location_name;

  // Set the current state to the last state of the pick trajectory
  tesseract_->getTesseract()->getEnvironment()->setState(joint_names, response.joint_trajectory.trajectory.bottomRows(1).transpose());

  if (action == nullptr){
      if (rviz_)
        {
          // Now update rviz environment
          if (!sendRvizChanges())
            exit(1);
        }
      return DEFAULT_VKC_GROUP_ID;
    }
  if (action->getActionType() == ActionType::PickAction)
  {
    PickAction::Ptr pick_act = std::dynamic_pointer_cast<PickAction>(action);
    location_name = pick_act->getAttachedObject();
    if (attach_locations_.find(location_name) == attach_locations_.end())
    {
      ROS_ERROR("Cannot find attach location %s inside environment!", location_name.c_str());
    }
    attachObject(location_name);
  }
  else if (action->getActionType() == ActionType::PlaceAction)
  {
    PlaceAction::Ptr place_act = std::dynamic_pointer_cast<PlaceAction>(action);
    location_name = place_act->getDetachedObject();
    std::cout << "detach: " << location_name << std::endl;
    detachObject(location_name);
  }
  else if (action->getActionType() == ActionType::UseAction)
  {
    UseAction::Ptr use_act = std::dynamic_pointer_cast<UseAction>(action);
    location_name = use_act->getAttachedObject();
    // std::cout << location_name << std::endl;
    Eigen::Isometry3d* tf = &(use_act->getTransform());
    attachObject(location_name, tf);
  }

  if (rviz_)
  {
    // Now update rviz environment
    if (!sendRvizChanges())
      exit(1);
  }

  if (action->getActionType() != ActionType::GotoAction)
  {
    tesseract_->getTesseract()->getSRDFModel()->removeGroup(DEFAULT_VKC_GROUP_ID);

    SRDFModel::Group group;
    group.name_ = DEFAULT_VKC_GROUP_ID;
    group.chains_.push_back(std::pair<std::string, std::string>("world", end_effector_link_));
    tesseract_->getTesseract()->getSRDFModel()->addGroup(group);

    tesseract_->getTesseract()->clearKinematics();
    tesseract_->getTesseract()->registerDefaultFwdKinSolvers();
    tesseract_->getTesseract()->registerDefaultInvKinSolvers();
  }

  return DEFAULT_VKC_GROUP_ID;
}

bool VKCEnvBasic::isGroupExist(std::string group_id)
{
  bool isfound_group = false;

  for (const auto& groups_ : tesseract_->getTesseract()->getSRDFModel()->getGroups())
  {
    if (groups_.name_ == group_id)
      isfound_group = true;
  }

  return isfound_group;
}

}  // namespace vkc