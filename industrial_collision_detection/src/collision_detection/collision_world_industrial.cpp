/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Ioan Sucan */

#include <industrial_collision_detection/collision_detection/collision_world_industrial.h>
#include <boost/bind.hpp>
#include <fcl/shape/geometric_shape_to_BVH_model.h>
#include <fcl/traversal/traversal_node_bvhs.h>
#include <fcl/traversal/traversal_node_setup.h>
#include <fcl/collision_node.h>
#include <console_bridge/console.h>

collision_detection::CollisionWorldIndustrial::CollisionWorldIndustrial() :
  CollisionWorld()
{
  fcl::DynamicAABBTreeCollisionManager* m = new fcl::DynamicAABBTreeCollisionManager();
  // m->tree_init_level = 2;
  manager_.reset(m);

  // request notifications about changes to new world
  observer_handle_ = getWorld()->addObserver(boost::bind(&CollisionWorldIndustrial::notifyObjectChange, this, _1, _2));
}

collision_detection::CollisionWorldIndustrial::CollisionWorldIndustrial(const WorldPtr& world) :
  CollisionWorld(world)
{
  fcl::DynamicAABBTreeCollisionManager* m = new fcl::DynamicAABBTreeCollisionManager();
  // m->tree_init_level = 2;
  manager_.reset(m);

  // request notifications about changes to new world
  observer_handle_ = getWorld()->addObserver(boost::bind(&CollisionWorldIndustrial::notifyObjectChange, this, _1, _2));
  getWorld()->notifyObserverAllObjects(observer_handle_, World::CREATE);
}

collision_detection::CollisionWorldIndustrial::CollisionWorldIndustrial(const CollisionWorldIndustrial &other, const WorldPtr& world) :
  CollisionWorld(other, world)
{
  fcl::DynamicAABBTreeCollisionManager* m = new fcl::DynamicAABBTreeCollisionManager();
  // m->tree_init_level = 2;
  manager_.reset(m);

  fcl_objs_ = other.fcl_objs_;
  for (std::map<std::string, FCLObject>::iterator it = fcl_objs_.begin() ; it != fcl_objs_.end() ; ++it)
    it->second.registerTo(manager_.get());
  // manager_->update();

  // request notifications about changes to new world
  observer_handle_ = getWorld()->addObserver(boost::bind(&CollisionWorldIndustrial::notifyObjectChange, this, _1, _2));
}

collision_detection::CollisionWorldIndustrial::~CollisionWorldIndustrial()
{
  getWorld()->removeObserver(observer_handle_);
}

void collision_detection::CollisionWorldIndustrial::checkRobotCollision(const CollisionRequest &req, CollisionResult &res, const CollisionRobot &robot, const robot_state::RobotState &state) const
{
  checkRobotCollisionHelper(req, res, robot, state, NULL);
}

void collision_detection::CollisionWorldIndustrial::checkRobotCollision(const CollisionRequest &req, CollisionResult &res, const CollisionRobot &robot, const robot_state::RobotState &state, const AllowedCollisionMatrix &acm) const
{
  checkRobotCollisionHelper(req, res, robot, state, &acm);
}

void collision_detection::CollisionWorldIndustrial::checkRobotCollision(const CollisionRequest &req, CollisionResult &res, const CollisionRobot &robot, const robot_state::RobotState &state1, const robot_state::RobotState &state2) const
{
  logError("FCL continuous collision checking not yet implemented");
}

void collision_detection::CollisionWorldIndustrial::checkRobotCollision(const CollisionRequest &req, CollisionResult &res, const CollisionRobot &robot, const robot_state::RobotState &state1, const robot_state::RobotState &state2, const AllowedCollisionMatrix &acm) const
{
  logError("FCL continuous collision checking not yet implemented");
}

void collision_detection::CollisionWorldIndustrial::checkRobotCollisionHelper(const CollisionRequest &req, CollisionResult &res, const CollisionRobot &robot, const robot_state::RobotState &state, const AllowedCollisionMatrix *acm) const
{
  // Don't do anything if the world is empty
  if (fcl_objs_.size() == 0)
    return;

  const CollisionRobotIndustrial &robot_fcl = dynamic_cast<const CollisionRobotIndustrial&>(robot);
  FCLObject fcl_obj;
  robot_fcl.constructFCLObject(state, fcl_obj);

  CollisionData cd(&req, &res, acm);
  cd.enableGroup(robot.getRobotModel());
  for (std::size_t i = 0 ; !cd.done_ && i < fcl_obj.collision_objects_.size() ; ++i)
    manager_->collide(fcl_obj.collision_objects_[i].get(), &cd, &collisionCallback);

  if (req.distance)
  {
    DistanceRequest dreq;
    dreq.enable_nearest_points = false;
    dreq.enable_signed_distance = true;
    dreq.group_name = req.group_name;
    dreq.type = DistanceRequestType::GLOBAL;
    dreq.acm = acm;
    DistanceResult dres;

    dreq.enableGroup(robot.getRobotModel());
    distanceRobotHelper(dreq, dres, robot, state);
    res.distance = dres.minimum_distance.distance;
  }
}

void collision_detection::CollisionWorldIndustrial::checkWorldCollision(const CollisionRequest &req, CollisionResult &res, const CollisionWorld &other_world) const
{
  checkWorldCollisionHelper(req, res, other_world, NULL);
}

void collision_detection::CollisionWorldIndustrial::checkWorldCollision(const CollisionRequest &req, CollisionResult &res, const CollisionWorld &other_world, const AllowedCollisionMatrix &acm) const
{
  checkWorldCollisionHelper(req, res, other_world, &acm);
}

void collision_detection::CollisionWorldIndustrial::checkWorldCollisionHelper(const CollisionRequest &req, CollisionResult &res, const CollisionWorld &other_world, const AllowedCollisionMatrix *acm) const
{
  const CollisionWorldIndustrial &other_fcl_world = dynamic_cast<const CollisionWorldIndustrial&>(other_world);
  CollisionData cd(&req, &res, acm);
  manager_->collide(other_fcl_world.manager_.get(), &cd, &collisionCallback);

  if (req.distance)
    res.distance = distanceWorldHelper(other_world, acm);
}

void collision_detection::CollisionWorldIndustrial::constructFCLObject(const World::Object *obj, FCLObject &fcl_obj) const
{
  for (std::size_t i = 0 ; i < obj->shapes_.size() ; ++i)
  {
    FCLGeometryConstPtr g = createCollisionGeometry(obj->shapes_[i], obj);
    if (g)
    {
      fcl::CollisionObject *co = new fcl::CollisionObject(g->collision_geometry_,  transform2fcl(obj->shape_poses_[i]));
      fcl_obj.collision_objects_.push_back(std::shared_ptr<fcl::CollisionObject>(co));
      fcl_obj.collision_geometry_.push_back(g);
    }
  }
}

void collision_detection::CollisionWorldIndustrial::updateFCLObject(const std::string &id)
{
  // remove FCL objects that correspond to this object
  std::map<std::string, FCLObject>::iterator jt = fcl_objs_.find(id);
  if (jt != fcl_objs_.end())
  {
    jt->second.unregisterFrom(manager_.get());
    jt->second.clear();
  }

  // check to see if we have this object
  collision_detection::World::const_iterator it = getWorld()->find(id);
  if (it != getWorld()->end())
  {
    // construct FCL objects that correspond to this object
    if (jt != fcl_objs_.end())
    {
      constructFCLObject(it->second.get(), jt->second);
      jt->second.registerTo(manager_.get());
    }
    else
    {
      constructFCLObject(it->second.get(), fcl_objs_[id]);
      fcl_objs_[id].registerTo(manager_.get());
    }
  }
  else
  {
    if (jt != fcl_objs_.end())
      fcl_objs_.erase(jt);
  }

  // manager_->update();
}

void collision_detection::CollisionWorldIndustrial::setWorld(const WorldPtr& world)
{
  if (world == getWorld())
    return;

  // turn off notifications about old world
  getWorld()->removeObserver(observer_handle_);

  // clear out objects from old world
  manager_->clear();
  fcl_objs_.clear();
  cleanCollisionGeometryCache();

  CollisionWorld::setWorld(world);

  // request notifications about changes to new world
  observer_handle_ = getWorld()->addObserver(boost::bind(&CollisionWorldIndustrial::notifyObjectChange, this, _1, _2));

  // get notifications any objects already in the new world
  getWorld()->notifyObserverAllObjects(observer_handle_, World::CREATE);
}

void collision_detection::CollisionWorldIndustrial::notifyObjectChange(const ObjectConstPtr& obj, World::Action action)
{
  if (action == World::DESTROY)
  {
    std::map<std::string, FCLObject>::iterator it = fcl_objs_.find(obj->id_);
    if (it != fcl_objs_.end())
    {
      it->second.unregisterFrom(manager_.get());
      it->second.clear();
      fcl_objs_.erase(it);
    }
    cleanCollisionGeometryCache();
  }
  else
  {
    updateFCLObject(obj->id_);
    if (action & (World::DESTROY|World::REMOVE_SHAPE))
      cleanCollisionGeometryCache();
  }
}

double collision_detection::CollisionWorldIndustrial::distanceRobotHelper(const CollisionRobot &robot, const robot_state::RobotState &state, const AllowedCollisionMatrix *acm) const
{
  // Don't do anything if the world is empty
  if (fcl_objs_.size() == 0)
    return std::numeric_limits<double>::max();

  const CollisionRobotIndustrial& robot_fcl = dynamic_cast<const CollisionRobotIndustrial&>(robot);
  FCLObject fcl_obj;
  robot_fcl.constructFCLObject(state, fcl_obj);

  CollisionRequest req;
  CollisionResult res;
  CollisionData cd(&req, &res, acm);
  cd.enableGroup(robot.getRobotModel());

  for(std::size_t i = 0; !cd.done_ && i < fcl_obj.collision_objects_.size(); ++i)
    manager_->distance(fcl_obj.collision_objects_[i].get(), &cd, &distanceCallback);


  return res.distance;
}

void collision_detection::CollisionWorldIndustrial::distanceRobotHelper(const DistanceRequest &req, DistanceResult &res, const collision_detection::CollisionRobot &robot, const robot_state::RobotState &state) const
{
  const CollisionRobotIndustrial& robot_fcl = dynamic_cast<const CollisionRobotIndustrial&>(robot);
  FCLObject fcl_obj;
  robot_fcl.constructFCLObject(state, fcl_obj);

  DistanceData drd(&req, &res);
  for(std::size_t i = 0; !drd.done && i < fcl_obj.collision_objects_.size(); ++i)
    manager_->distance(fcl_obj.collision_objects_[i].get(), &drd, &distanceDetailedCallback);

}

double collision_detection::CollisionWorldIndustrial::distanceRobot(const CollisionRobot &robot, const robot_state::RobotState &state,
		bool verbose) const
{
  return distanceRobotHelper(robot, state, NULL);
}

double collision_detection::CollisionWorldIndustrial::distanceRobot(const CollisionRobot &robot, const robot_state::RobotState &state,
		const AllowedCollisionMatrix &acm,bool verbose) const
{
  return distanceRobotHelper(robot, state, &acm);
}

void collision_detection::CollisionWorldIndustrial::distanceRobot(const DistanceRequest &req, DistanceResult &res,
		const collision_detection::CollisionRobot &robot, const robot_state::RobotState &state,bool verbose) const
{
  distanceRobotHelper(req, res, robot, state);
}

void collision_detection::CollisionWorldIndustrial::distanceRobot(const DistanceRequest& req, DistanceResult& res, const CollisionRobot& robot,
                             const robot_state::RobotState& state) const
{
  distanceRobotHelper(req, res, robot, state);
}

double collision_detection::CollisionWorldIndustrial::distanceWorld(const CollisionWorld &world,bool verbose) const
{
  return distanceWorldHelper(world, NULL);
}

double collision_detection::CollisionWorldIndustrial::distanceWorld(const CollisionWorld &world, const AllowedCollisionMatrix &acm,
		bool verbose) const
{
  return distanceWorldHelper(world, &acm);
}

void collision_detection::CollisionWorldIndustrial::distanceWorld(const DistanceRequest& req, DistanceResult& res, const CollisionWorld& world) const
{
  const CollisionWorldIndustrial& other_fcl_world = dynamic_cast<const CollisionWorldIndustrial&>(world);
  DistanceData drd(&req,&res);
  manager_->distance(other_fcl_world.manager_.get(), &drd, &distanceCallback);
}

double collision_detection::CollisionWorldIndustrial::distanceWorldHelper(const CollisionWorld &other_world, const AllowedCollisionMatrix *acm) const
{
  const CollisionWorldIndustrial& other_fcl_world = dynamic_cast<const CollisionWorldIndustrial&>(other_world);
  CollisionRequest req;
  CollisionResult res;
  CollisionData cd(&req, &res, acm);
  manager_->distance(other_fcl_world.manager_.get(), &cd, &distanceCallback);

  return res.distance;
}

