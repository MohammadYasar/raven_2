/*
 * end_effector_grasp.cpp
 *
 *  Created on: Oct 10, 2012
 *      Author: benk
 */

#include <raven/control/input/end_effector_grasp_input.h>

std::vector<float>
EndEffectorGraspInput::values() const {
	std::vector<float> values;
	for (size_t i=0;i<arms_.size();i++) {
		values.push_back(arms_[i].value());
	}
	return values;
}

void
EndEffectorGraspInput::setFrom(DeviceConstPtr dev) {
	FOREACH_ARM_IN_CONST_DEVICE(arm_in,dev) {
		EndEffectorGraspData& arm_curr = armById(arm_in->id());
		arm_curr.value() = arm_in->getJointById(Joint::IdType::GRASP_)->position();
	}
}

void
SingleArmEndEffectorGraspInput::setFrom(DeviceConstPtr dev) {
	data().value() = dev->getArmById(id())->getJointById(Joint::IdType::GRASP_)->position();
}
