/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: ScriptConditions.cpp /////////////////////////////////////////////////////////////////////////
// The game scripting conditions.  Evaluates conditions.
// Author: John Ahlquist, Nov. 2001
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameEngine.h"
#include "Common/MapObject.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Team.h"
#include "Common/ObjectStatusTypes.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"
#include "GameClient/View.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectTypes.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptConditions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/VictoryConditions.h"


class ObjectTypesTemp
{
public:
	ObjectTypes* m_types;

	ObjectTypesTemp() : m_types(nullptr)
	{
		m_types = newInstance(ObjectTypes);
	}

	~ObjectTypesTemp()
	{
		deleteInstance(m_types);
	}
};

// STATICS ////////////////////////////////////////////////////////////////////////////////////////
namespace rts
{
	template<typename T>
		T sum(std::vector<T>& vecOfValues )
	{
		T retVal = 0;
		typename std::vector<T>::iterator it;
		for (it = vecOfValues.begin(); it != vecOfValues.end(); ++it) {
			retVal += (*it);
		}
		return retVal;
	}
};

// GLOBALS ////////////////////////////////////////////////////////////////////////////////////////
ScriptConditionsInterface *TheScriptConditions = nullptr;

class TransportStatus : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TransportStatus, "TransportStatus")
public:
	TransportStatus *	m_nextStatus;
	ObjectID					m_objID;
	UnsignedInt				m_frameNumber;
	Int								m_unitCount;

public:
	TransportStatus() : m_objID(INVALID_ID), m_frameNumber(0), m_unitCount(0), m_nextStatus(nullptr) {}
	//~TransportStatus();
};

//-------------------------------------------------------------------------------------------------
TransportStatus::~TransportStatus()
{
	deleteInstance(m_nextStatus);
}

//-------------------------------------------------------------------------------------------------
static TransportStatus *s_transportStatuses;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ScriptConditions::ScriptConditions()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ScriptConditions::~ScriptConditions()
{
	reset(); // just in case.
}

//-------------------------------------------------------------------------------------------------
/** Init */
//-------------------------------------------------------------------------------------------------
void ScriptConditions::init()
{

	reset();

}

//-------------------------------------------------------------------------------------------------
/** Reset */
//-------------------------------------------------------------------------------------------------
void ScriptConditions::reset()
{

	deleteInstance(s_transportStatuses);
	s_transportStatuses = nullptr;
	// Empty for now.  jba.
}

//-------------------------------------------------------------------------------------------------
/** Update */
//-------------------------------------------------------------------------------------------------
void ScriptConditions::update()
{

	// Empty for now. jba
}


//-------------------------------------------------------------------------------------------------
/** Finds the player by the name in the parameter, and if found caches the player mask in the
parameter so we don't have to do a name search.  May return null if the player doesn't exist.*/
//-------------------------------------------------------------------------------------------------
Player *ScriptConditions::playerFromParam(Parameter *pSideParm)
{
	DEBUG_ASSERTCRASH(Parameter::SIDE == pSideParm->getParameterType(), ("Wrong parameter type."));
	Player *pPlayer=nullptr;
	UnsignedInt mask = (UnsignedInt)pSideParm->getInt();
	if (mask) {
		pPlayer = ThePlayerList->getPlayerFromMask(mask);
	} else {
		pPlayer = TheScriptEngine->getPlayerFromAsciiString(pSideParm->getString());
		if (pPlayer) {
			// Enemy player can change dynamically, so don't cache the player mask.  jba.
			if (pSideParm->getString()!=THIS_PLAYER_ENEMY) {
				mask = pPlayer->getPlayerMask();
			}
		} else {
			mask = 0xFFFF0000;
		}
		pSideParm->friend_setInt((Int)mask);
	}
	DEBUG_ASSERTCRASH(pPlayer, ("Couldn't find player %s", pSideParm->getString().str()));
	return pPlayer;
}

//-------------------------------------------------------------------------------------------------
/** objectTypesFromParam */
//-------------------------------------------------------------------------------------------------
void ScriptConditions::objectTypesFromParam(Parameter *pTypeParm, ObjectTypes *outObjectTypes)
{
	if (!(pTypeParm && outObjectTypes)) {
		return;
	}

	AsciiString str = pTypeParm->getString();

	if (str.isEmpty()) {
		return;
	}

	ObjectTypes *types = TheScriptEngine->getObjectTypes(str);
	if (!types) {
		(*outObjectTypes).addObjectType(str);
	} else {
		(*outObjectTypes) = (*types);
	}
}


//-------------------------------------------------------------------------------------------------
/** evaluateAllDestroyed */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateAllDestroyed(Parameter *pSideParm )
{
	Player *pPlayer = playerFromParam(pSideParm);
	if (pPlayer) {
		return (!pPlayer->hasAnyObjects());
	}
	return true; // Non existent player is all destroyed. :)
}

//-------------------------------------------------------------------------------------------------
/** evaluateAllBuildFacilitiesDestroyed */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateAllBuildFacilitiesDestroyed(Parameter *pSideParm )
{
	Player *pPlayer = playerFromParam(pSideParm);
	if (pPlayer) {
		return (!pPlayer->hasAnyBuildFacility());
	}
	return true; // Non existent player is all destroyed. :)
}

//-------------------------------------------------------------------------------------------------
/** evaluateIsDestroyed */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateIsDestroyed(Parameter *pTeamParm)
{
	Team *theTeam = TheScriptEngine->getTeamNamed( pTeamParm->getString() );
	// The team is the team based on the name, and the calling team (if any) and the team that
	// is being considered for the condition.  jba. :)
	if (theTeam) {
		return (!theTeam->hasAnyObjects());
	}
	return false; // Non existent team is not destroyed.
}

//-------------------------------------------------------------------------------------------------
/** evaluateBridgeBroken */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateBridgeBroken(Parameter *pBridgeParm)
{
	if (!TheTerrainLogic->anyBridgesDamageStatesChanged()) {
		// Don't bother checking if no bridges changed damage states.
		return false;
	}
	Object *theBridge = TheScriptEngine->getUnitNamed( pBridgeParm->getString() );
	if (theBridge) {
		return (TheTerrainLogic->isBridgeBroken(theBridge));
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateBridgeRepaired */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateBridgeRepaired(Parameter *pBridgeParm)
{
	if (!TheTerrainLogic->anyBridgesDamageStatesChanged()) {
		// Don't bother checking if no bridges changed damage states.
		return false;
	}
	Object *theBridge = TheScriptEngine->getUnitNamed( pBridgeParm->getString() );
	if (theBridge) {
		return (TheTerrainLogic->isBridgeRepaired(theBridge));
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedUnitDestroyed */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedUnitDestroyed(Parameter *pUnitParm)
{
	Object *theUnit = TheScriptEngine->getUnitNamed( pUnitParm->getString() );
	if (theUnit)
	{
		return theUnit->isEffectivelyDead();
	}

	if (TheScriptEngine->didUnitExist(pUnitParm->getString())) {
		return true;
	}
	return false; // Non existent unit is not destroyed.
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedUnitExists */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedUnitExists(Parameter *pUnitParm)
{
	Object *theUnit = TheScriptEngine->getUnitNamed( pUnitParm->getString() );
	if (theUnit)
	{
		return !theUnit->isEffectivelyDead();
	}

	return false; // Doesn't exist.
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedUnitDying */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedUnitDying(Parameter *pUnitParm)
{
	Object *theUnit = TheScriptEngine->getUnitNamed( pUnitParm->getString() );
	if (theUnit)
	{
		return theUnit->isEffectivelyDead();
	}

	if (TheScriptEngine->didUnitExist(pUnitParm->getString()))
	{
		return false; // already totally killed
	}
	return false; // Non existent unit is not dying.
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedUnitTotallyDead */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedUnitTotallyDead(Parameter *pUnitParm)
{
	Object *theUnit = TheScriptEngine->getUnitNamed( pUnitParm->getString() );
	if (theUnit) {
		return false; // if the unit still exists, it isn't totally dead.
	}

	if (TheScriptEngine->didUnitExist(pUnitParm->getString())) {
		// Did exist, now it doesnt.  So it is really, really dead.
		return true; // totally killed
	}
	return false; // Non existent unit is not dead.
}

//-------------------------------------------------------------------------------------------------
/** evaluateHasUnits */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateHasUnits(Parameter *pTeamParm)
{
	AsciiString desiredTeamName = pTeamParm->getString();
	// If they are calling a <this team> condition, do it.
	if (desiredTeamName == THIS_TEAM) {
		Team *theTeam = TheScriptEngine->getTeamNamed( desiredTeamName );
		// The team is the team based on the name, and the calling team (if any) and the team that
		// is being considered for the condition.  jba. :)
		if (theTeam) {
			return (theTeam->hasAnyUnits());
		}
		return false; // Non existent team has no units.
	}
	Team *thisTeam = TheScriptEngine->getTeamNamed(THIS_TEAM);
	if (thisTeam && thisTeam->getName()==desiredTeamName)	{
		return thisTeam->hasAnyUnits();
	}

	// It isn't THIS_TEAM, and doesn't match the THIS_TEAM, so check if any team with this name
	// has units.
	TeamPrototype *pProto = nullptr;
	pProto = TheTeamFactory->findTeamPrototype(desiredTeamName);

	if (pProto) {
		// We have a team referred to in the conditions.  Iterate over the instances of the team,
		// applying the script conditions (and possibly actions) to each instance of the team.
		for (DLINK_ITERATOR<Team> iter = pProto->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			if (iter.cur()->hasAnyUnits()) {
				return true;
			}
		}
	}
	return false; // Non existent team has no units.
}

//-------------------------------------------------------------------------------------------------
/** evaluateUnitsEntered */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamInsideAreaPartially(Parameter *pTeamParm, Parameter *pTriggerAreaParm, Parameter *pTypeParm)
{
	Team *theTeam = TheScriptEngine->getTeamNamed( pTeamParm->getString() );
	// The team is the team based on the name, and the calling team (if any) and the team that
	// is being considered for the condition.  jba. :)
	AsciiString triggerName = pTriggerAreaParm->getString();
	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerAreaParm->getString());

	if (pTrig == nullptr) return false;
	if (theTeam) {
		return (theTeam->someInsideSomeOutside(pTrig, (UnsignedInt) pTypeParm->getInt()) ||
						theTeam->allInside(pTrig, (UnsignedInt) pTypeParm->getInt()));
	}
	return false; // Non existent team isn't in trigger area. :)
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedInsideArea */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedInsideArea(Parameter *pUnitParm, Parameter *pTriggerAreaParm )
{
	Object *theObj = TheScriptEngine->getUnitNamed( pUnitParm->getString() );

	if (!theObj) {
		return false;
	}

	AsciiString triggerName = pTriggerAreaParm->getString();
	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerAreaParm->getString());
	if (pTrig == nullptr) return false;
	if (theObj) {
		Coord3D pCoord = *theObj->getPosition();
		ICoord3D iCoord;
		iCoord.x = pCoord.x; iCoord.y = pCoord.y; iCoord.z = pCoord.z;
		return pTrig->pointInTrigger(iCoord);
	}
	return false; // Non existent team isn't in trigger area. :)
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerHasUnitTypeInArea */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasUnitTypeInArea(Condition *pCondition, Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm, Parameter *pTypeParm, Parameter *pTriggerParm )
{
	AsciiString triggerName = pTriggerParm->getString();
	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (pTrig == nullptr) return false;

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}
	Player::PlayerTeamList::const_iterator it;
	Bool anyChanges = false;


	if (pCondition->getCustomData() == 0) anyChanges = true;

	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		if (anyChanges) break;
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			if (anyChanges) break;
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			if (team->didEnterOrExit()) {
				anyChanges = true;
			}
		}
	}

	if (TheScriptEngine->getFrameObjectCountChanged() > pCondition->getCustomFrame()) {
		anyChanges = true; // Objects were added/deleted last frame, so count could have changed.  jba.
	}

	if (!anyChanges) {
		if (pCondition->getCustomData()==-1) return false;
		if (pCondition->getCustomData()==1) return true;
	}

	ObjectTypesTemp types;
	objectTypesFromParam(pTypeParm, types.m_types);

	Int count = 0;
	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object *pObj = iter.cur();
				if (!pObj) {
					continue;
				}

				if (types.m_types->isInSet(pObj->getTemplate())) {
					if (pObj->isInside(pTrig)) {

						//
						// dead objects will not be considered, except crates ... they are "dead" cause
						// they have no body and health, but are a class of object we want to
						// trigger this stuff
						//
						if (!(pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) || pObj->isKindOf( KINDOF_CRATE ) ) {
							count++;
						}
					}
				}
			}
		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN :			comparison = (count < pCountParm->getInt()); break;
		case Parameter::LESS_EQUAL :		comparison = (count <= pCountParm->getInt()); break;
		case Parameter::EQUAL :					comparison = (count == pCountParm->getInt()); break;
		case Parameter::GREATER_EQUAL :	comparison = (count >= pCountParm->getInt()); break;
		case Parameter::GREATER :				comparison = (count > pCountParm->getInt()); break;
		case Parameter::NOT_EQUAL :			comparison = (count != pCountParm->getInt()); break;
	}
	pCondition->setCustomData(-1); // false.
	if (comparison) {
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerHasUnitKindInArea */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasUnitKindInArea(Condition *pCondition, Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm, Parameter *pKindParm, Parameter *pTriggerParm )
{
	AsciiString triggerName = pTriggerParm->getString();
	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (pTrig == nullptr) return false;

	KindOfType kind = (KindOfType)pKindParm->getInt();

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	Player::PlayerTeamList::const_iterator it;
	Bool anyChanges = false;
	if (pCondition->getCustomData() == 0) anyChanges = true;


	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		if (anyChanges) break;
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			if (anyChanges) break;
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			if (team->didEnterOrExit()) {
				anyChanges = true;
			}
		}
	}
	if (TheScriptEngine->getFrameObjectCountChanged() > pCondition->getCustomFrame()) {
		anyChanges = true; // Objects were added/deleted since we cached, so count could have changed.  jba.
	}
	if (!anyChanges) {
		if (pCondition->getCustomData()==-1) return false;
		if (pCondition->getCustomData()==1) return true;

	}


	Int count = 0;
	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object *pObj = iter.cur();
				if (!pObj) {
					continue;
				}
				if (pObj->isKindOf(kind)) {
					if (pObj->isInside(pTrig)) {
						if (!(pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT))) {
							count++;
						}
					}
				}
			}
		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN :			comparison = (count < pCountParm->getInt()); break;
		case Parameter::LESS_EQUAL :		comparison = (count <= pCountParm->getInt()); break;
		case Parameter::EQUAL :					comparison = (count == pCountParm->getInt()); break;
		case Parameter::GREATER_EQUAL :	comparison = (count >= pCountParm->getInt()); break;
		case Parameter::GREATER :				comparison = (count > pCountParm->getInt()); break;
		case Parameter::NOT_EQUAL :			comparison = (count != pCountParm->getInt()); break;
	}
	pCondition->setCustomData(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamStateIs */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamStateIs(Parameter *pTeamParm, Parameter *pStateParm )
{
	Team *theTeam = TheScriptEngine->getTeamNamed( pTeamParm->getString() );
	// The team is the team based on the name, and the calling team (if any) and the team that
	// is being considered for the condition.  jba. :)
	AsciiString stateName = pStateParm->getString();
	if (theTeam) {
		return (theTeam->getState() == stateName);
	}
	return false; // Non existent team isn't in any state.
}


//-------------------------------------------------------------------------------------------------
/** evaluateTeamStateIsNot */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamStateIsNot(Parameter *pTeamParm, Parameter *pStateParm )
{
	Team *theTeam = TheScriptEngine->getTeamNamed( pTeamParm->getString() );
	// The team is the team based on the name, and the calling team (if any) and the team that
	// is being considered for the condition.  jba. :)
	AsciiString stateName = pStateParm->getString();
	if (theTeam) {
		return (!(theTeam->getState() == stateName));
	}
	return false; // Non existent team isn't in any state.
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedOutsideArea */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedOutsideArea(Parameter *pUnitParm, Parameter *pTriggerParm)
{// This is actually NamedUnitInside(...)

	return !evaluateNamedInsideArea(pUnitParm, pTriggerParm);
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamInsideAreaEntirely */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamInsideAreaEntirely(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm)
{// This is actually TeamInside(...)
	Team *theTeam = TheScriptEngine->getTeamNamed( pTeamParm->getString() );
	// The team is the team based on the name, and the calling team (if any) and the team that
	// is being considered for the condition.  jba. :)
	AsciiString triggerName = pTriggerParm->getString();
	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());

	if (pTrig == nullptr)
		return false;

	if (theTeam) {
		return theTeam->allInside(pTrig, (UnsignedInt)pTypeParm->getInt());
	}
	return false; // Non existent team isn't in trigger area. :)
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamOutsideAreaEntirely */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamOutsideAreaEntirely(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm)
{
	return !(evaluateTeamInsideAreaEntirely(pTeamParm, pTriggerParm, pTypeParm) ||
					 evaluateTeamInsideAreaPartially(pTeamParm, pTriggerParm, pTypeParm));
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedAttackedByType */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedAttackedByType(Parameter *pUnitParm, Parameter *pTypeParm)
{
	Object *theObj = TheScriptEngine->getUnitNamed( pUnitParm->getString() );
	if (!theObj) {
		return false;
	}

	BodyModuleInterface* theBodyModule = theObj->getBodyModule();
	if (!theBodyModule) {
		return false;
	}

	const DamageInfo* lastDamageInfo = theBodyModule->getLastDamageInfo();

	if (!lastDamageInfo) {
		return false;
	}

	const ThingTemplate *attackerTemplate = lastDamageInfo->in.m_sourceTemplate;
	if( attackerTemplate )
	{
		//New system... we don't care if the attacker is alive or dead... we just want the type right?
		ObjectTypesTemp types;
		objectTypesFromParam(pTypeParm, types.m_types);
		if( types.m_types->isInSet( attackerTemplate ) )
		{
			return TRUE;
		}
	}
	else
	{
		//Old system... just incase m_sourceTemplate doesn't get set, don't want to foobar the logic.
		ObjectID id = lastDamageInfo->in.m_sourceID;
		Object* pAttacker = TheGameLogic->findObjectByID(id);
		if (!pAttacker || !pAttacker->getTemplate())
		{
			return FALSE;
		}
		ObjectTypesTemp types;
		objectTypesFromParam(pTypeParm, types.m_types);
		if( types.m_types->isInSet( pAttacker->getTemplate()->getName() ) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamAttackedByType */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamAttackedByType(Parameter *pTeamParm, Parameter *pTypeParm)
{
	Team *theTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!theTeam) {
		return FALSE;
	}

	ObjectTypesTemp types;
	objectTypesFromParam(pTypeParm, types.m_types);

	for (DLINK_ITERATOR<Object> iter = theTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
		Object *pCur = iter.cur();
		if (!pCur) {
			continue;
		}

		BodyModuleInterface* theBodyModule = pCur->getBodyModule();
		if (!theBodyModule) {
			continue;
		}

		const DamageInfo* lastDamageInfo = theBodyModule->getLastDamageInfo();

		if (!lastDamageInfo) {
			continue;
		}

		ObjectTypesTemp types;
		objectTypesFromParam(pTypeParm, types.m_types);

		const ThingTemplate *attackerTemplate = lastDamageInfo->in.m_sourceTemplate;
		if( attackerTemplate )
		{
			//New system... we don't care if the attacker is alive or dead... we just want the type right?
			if( types.m_types->isInSet( attackerTemplate ) )
			{
				return TRUE;
			}
		}
		else
		{
			//Old system... just incase m_sourceTemplate doesn't get set, don't want to foobar the logic.
			ObjectID id = lastDamageInfo->in.m_sourceID;
			Object* pAttacker = TheGameLogic->findObjectByID(id);
			if (!pAttacker || !pAttacker->getTemplate())
			{
				//Kris: Woah... do not return false here... because we need to iterate other team members!
				//return FALSE;
				continue;
			}
			if( types.m_types->isInSet( pAttacker->getTemplate()->getName() ) )
			{
				return TRUE;
			}
		}

	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedAttackedByPlayer */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedAttackedByPlayer(Parameter *pUnitParm, Parameter *pPlayerParm)
{
	Object *theObj = TheScriptEngine->getUnitNamed( pUnitParm->getString() );
	if (!theObj) {
		return false;
	}

	BodyModuleInterface* theBodyModule = theObj->getBodyModule();
	if (!theBodyModule) {
		return false;
	}

	const DamageInfo* lastDamageInfo = theBodyModule->getLastDamageInfo();

	if (!lastDamageInfo) {
		return false;
	}

	ObjectID id = lastDamageInfo->in.m_sourceID;
	Object* pAttacker = TheGameLogic->findObjectByID(id);
	Player *pPlayer = nullptr;
	if (lastDamageInfo->in.m_sourcePlayerMask) {
		pPlayer = ThePlayerList->getPlayerFromMask(lastDamageInfo->in.m_sourcePlayerMask);
	}
	if (pPlayer || pAttacker) {
		Player *victimPlayer = playerFromParam(pPlayerParm);
		if (pPlayer == victimPlayer) {
			return true;
		}
		if (!pAttacker) return false; // wasn't attacked.
		return (pAttacker->getControllingPlayer() == victimPlayer);
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamAttackedByPlayer */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamAttackedByPlayer(Parameter *pTeamParm, Parameter *pPlayerParm)
{
	Team *theTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!theTeam) {
		return false;
	}

	for (DLINK_ITERATOR<Object> iter = theTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
		Object *pCur = iter.cur();
		if (!pCur) {
			continue;
		}
		BodyModuleInterface* theBodyModule = pCur->getBodyModule();
		if (!theBodyModule) {
			continue;
		}

		const DamageInfo* lastDamageInfo = theBodyModule->getLastDamageInfo();

		if (!lastDamageInfo) {
			continue;
		}

		ObjectID id = lastDamageInfo->in.m_sourceID;
		Object* pAttacker = TheGameLogic->findObjectByID(id);
		if (!pAttacker) {
			continue;
		}

		if (pAttacker->getControllingPlayer() == playerFromParam(pPlayerParm)) {
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateBuiltByPlayer */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateBuiltByPlayer(Condition *pCondition, Parameter* pTypeParm, Parameter* pPlayerParm)
{
	if (pCondition->getCustomData()!=0) {
		// We have a cached value.
		if (TheScriptEngine->getFrameObjectCountChanged() == pCondition->getCustomFrame()) {
			// object count hasn't changed.  Use cached value.
			if (pCondition->getCustomData()==1) return true;
			if (pCondition->getCustomData()==-1) return false;
		}
	}
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	const ThingTemplate* pTemplate = TheThingFactory->findTemplate(pTypeParm->getString());
	if (!pTemplate) {
		return false;
	}

	ObjectTypesTemp types;
	objectTypesFromParam(pTypeParm, types.m_types);

	std::vector<Int> counts;
	std::vector<const ThingTemplate *> templates;

	Int numTemplates = types.m_types->prepForPlayerCounting(templates, counts);
	if (numTemplates > 0) {
		pPlayer->countObjectsByThingTemplate(numTemplates, &(*templates.begin()), false, &(*counts.begin()));
	} else {
		return 0;
	}

	Int sumOfObjs = rts::sum(counts);
	pCondition->setCustomData(-1); // false.
	if (sumOfObjs != 0) {
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return (sumOfObjs != 0);
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedCreated */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedCreated(Parameter* pUnitParm)
{
	// This is actually evaluateNamedExists(...)
	///@todo - evaluate created, not exists...
	return (TheScriptEngine->getUnitNamed(pUnitParm->getString()) != nullptr);
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamCreated */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamCreated(Parameter* pTeamParm)
{
	Team *pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (pTeam) {
		return pTeam->isCreated();
	}
	return ( false );
}

//-------------------------------------------------------------------------------------------------
/** evaluateUnitHealth */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateUnitHealth(Parameter *pUnitParm, Parameter* pComparisonParm, Parameter *pHealthPercent)
{
	Object *theObj = TheScriptEngine->getUnitNamed( pUnitParm->getString() );
	if (!theObj) {
		return false;
	}
	if (!theObj->getBodyModule()) return false;

	Real curHealth = theObj->getBodyModule()->getHealth();
	Real initialHealth = theObj->getBodyModule()->getInitialHealth();
	Int curPercent = (curHealth*100 + initialHealth/2)/initialHealth;

	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN :			return (curPercent < pHealthPercent->getInt());
		case Parameter::LESS_EQUAL :		return (curPercent <= pHealthPercent->getInt());
		case Parameter::EQUAL :					return (curPercent == pHealthPercent->getInt());
		case Parameter::GREATER_EQUAL :	return (curPercent >= pHealthPercent->getInt());
		case Parameter::GREATER :				return (curPercent > pHealthPercent->getInt());
		case Parameter::NOT_EQUAL :			return (curPercent != pHealthPercent->getInt());
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerHasCredits */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasCredits(Parameter *pCreditsParm, Parameter* pComparisonParm, Parameter *pPlayerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	if (pPlayer && pPlayer->getMoney()) {
		switch (pComparisonParm->getInt())
		{
			case Parameter::LESS_THAN :			return (pCreditsParm->getInt() < pPlayer->getMoney()->countMoney()); break;
			case Parameter::LESS_EQUAL :		return (pCreditsParm->getInt() <= pPlayer->getMoney()->countMoney()); break;
			case Parameter::EQUAL :					return (pCreditsParm->getInt() == pPlayer->getMoney()->countMoney()); break;
			case Parameter::GREATER_EQUAL :	return (pCreditsParm->getInt() >= pPlayer->getMoney()->countMoney()); break;
			case Parameter::GREATER :				return (pCreditsParm->getInt() > pPlayer->getMoney()->countMoney()); break;
			case Parameter::NOT_EQUAL :			return (pCreditsParm->getInt() != pPlayer->getMoney()->countMoney()); break;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateBuildingEntered */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateBuildingEntered( Parameter *pPlayerParm, Parameter *pItemParm )
{
	Object *theObj = TheScriptEngine->getUnitNamed( pItemParm->getString() );
	if (!theObj) {
		return false;
	}

	ContainModuleInterface *contain = theObj->getContain();
	if( !contain )
	{
		DEBUG_CRASH( ("evaluateBuildingEntered script condition -- building doesn't have a container.") );
		return false;
	}
	PlayerMaskType playerMask = theObj->getContain()->getPlayerWhoEntered();
	if (playerMask==0) {
		return false;
	}

	Player *pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}
	if (playerMask == pPlayer->getPlayerMask()) {
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateIsBuildingEmpty */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateIsBuildingEmpty( Parameter *pItemParm )
{

	Object *theBuilding = TheScriptEngine->getUnitNamed(pItemParm->getString());
	if (!theBuilding) {
		return false;
	}

	ContainModuleInterface* contain = theBuilding->getContain();
	if (!contain) {
		return false;
	}
	if (contain->getContainCount() > 0) {
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
/** evaluateEnemySighted */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateEnemySighted(Parameter *pItemParm, Parameter *pAllianceParm, Parameter* pPlayerParm)
{

	Object *theObj = TheScriptEngine->getUnitNamed( pItemParm->getString() );
	if (!theObj) {
		return false;
	}

	Player *pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	// filter out appropriately based on alliances
	Int relationDescriber;
	switch (pAllianceParm->getInt()) {
		case Parameter::REL_NEUTRAL:
			relationDescriber = PartitionFilterRelationship::ALLOW_NEUTRAL;
			break;
		case Parameter::REL_FRIEND:
			relationDescriber = PartitionFilterRelationship::ALLOW_ALLIES;
			break;
		case Parameter::REL_ENEMY:
			relationDescriber = PartitionFilterRelationship::ALLOW_ENEMIES;
			break;
		default:
			DEBUG_CRASH(("Unhandled case in ScriptConditions::evaluateEnemySighted()"));
			relationDescriber = 0;
			break;
	}
	PartitionFilterRelationship	filterTeam(theObj, relationDescriber);

	// and only stuff that is not dead
	PartitionFilterAlive filterAlive;

	// and only nonstealthed items.
	PartitionFilterRejectByObjectStatus filterStealth( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_STEALTHED ),
																										 MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_DETECTED, OBJECT_STATUS_DISGUISED ) );

	// and only on-map (or not)
	PartitionFilterSameMapStatus filterMapStatus(theObj);

	PartitionFilter *filters[] = { &filterTeam, &filterAlive, &filterStealth, &filterMapStatus, nullptr };

	Real visionRange = theObj->getVisionRange();

	SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange(
								theObj, visionRange, FROM_CENTER_2D, filters);
	MemoryPoolObjectHolder hold(iter);
	for (Object *them = iter->first(); them; them = iter->next())
	{
		if (them->getControllingPlayer() == pPlayer) {
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateTypeSighted */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTypeSighted(Parameter *pItemParm, Parameter *pTypeParm, Parameter* pPlayerParm)
{

	Object *theObj = TheScriptEngine->getUnitNamed( pItemParm->getString() );
	if (!theObj) {
		return false;
	}

	Player *pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	ObjectTypesTemp types;
	objectTypesFromParam(pTypeParm, types.m_types);

	// and only stuff that is not dead
	PartitionFilterAlive filterAlive;

	// and only nonstealthed items.
	PartitionFilterRejectByObjectStatus filterStealth( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_STEALTHED ),
																										 MAKE_OBJECT_STATUS_MASK2( OBJECT_STATUS_DETECTED, OBJECT_STATUS_DISGUISED ) );

	// and only on-map (or not)
	PartitionFilterSameMapStatus filterMapStatus(theObj);

	PartitionFilter *filters[] = { &filterAlive, &filterStealth, &filterMapStatus, nullptr };

	Real visionRange = theObj->getVisionRange();

	SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange(
								theObj, visionRange, FROM_CENTER_2D, filters);
	MemoryPoolObjectHolder hold(iter);
	for (Object *them = iter->first(); them; them = iter->next())
	{
		if (them->getControllingPlayer() == pPlayer) {
			if (types.m_types->isInSet(them->getTemplate()->getName())) 
				return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedDiscovered */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedDiscovered(Parameter *pItemParm, Parameter* pPlayerParm)
{
	Object *theObj = TheScriptEngine->getUnitNamed( pItemParm->getString() );
	if (!theObj) {
		return false;
	}

	Player *pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	// We are held, so we are not visible.
	if( theObj->isDisabledByType( DISABLED_HELD ) )
	{
		return false;
	}

	// If we are stealthed we are not visible.
	if( theObj->getStatusBits().test( OBJECT_STATUS_STEALTHED ) &&
			!theObj->getStatusBits().test( OBJECT_STATUS_DETECTED ) &&
			!theObj->getStatusBits().test( OBJECT_STATUS_DISGUISED ) )
	{
		return false;
	}
	ObjectShroudStatus shroud = theObj->getShroudedStatus(pPlayer->getPlayerIndex());
	return (shroud == OBJECTSHROUD_CLEAR || shroud == OBJECTSHROUD_PARTIAL_CLEAR);
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamDiscovered */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamDiscovered(Parameter *pTeamParm, Parameter *pPlayerParm)
{
	Team *theTeam = TheScriptEngine->getTeamNamed( pTeamParm->getString() );
	if (!theTeam) {
		return false;
	}

	Player *pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	for (DLINK_ITERATOR<Object> iter = theTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
		Object *pObj = iter.cur();
		if (!pObj) {
			continue;
		}

		// We are held, so we are not visible.
		if( pObj->isDisabledByType( DISABLED_HELD ) )
		{
			continue;
		}

		// If we are stealthed we are not visible.
		if( pObj->getStatusBits().test( OBJECT_STATUS_STEALTHED ) &&
				!pObj->getStatusBits().test( OBJECT_STATUS_DETECTED ) &&
				!pObj->getStatusBits().test( OBJECT_STATUS_DISGUISED ) )
		{
			continue;
		}
		ObjectShroudStatus shroud = pObj->getShroudedStatus(pPlayer->getPlayerIndex());

		if (shroud == OBJECTSHROUD_CLEAR || shroud == OBJECTSHROUD_PARTIAL_CLEAR) {
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateMissionAttempts */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMissionAttempts(Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pAttemptsParm)
{
//Player* pPlayer = playerFromParam(pPlayerParm);
	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedOwnedByPlayer */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedOwnedByPlayer(Parameter *pUnitParm, Parameter *pPlayerParm)
{

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	Object* pObj = TheScriptEngine->getUnitNamed(pUnitParm->getString());
	if (!pObj) {
		return false;
	}

	return (pObj->getControllingPlayer() == pPlayer);
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamOwnedByPlayer */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamOwnedByPlayer(Parameter *pTeamParm, Parameter *pPlayerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) {
		return false;
	}

	return (pTeam->getControllingPlayer() == pPlayer);
}


//-------------------------------------------------------------------------------------------------
/** evaluatePlayerHasNOrFewerBuildings */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasNOrFewerBuildings(Parameter *pBuildingCountParm, Parameter *pPlayerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	return (pBuildingCountParm->getInt() >= pPlayer->countBuildings());
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerHasNOrFewerFactionBuildings */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasNOrFewerFactionBuildings(Parameter *pBuildingCountParm, Parameter *pPlayerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	KindOfMaskType mask;
	mask.set(KINDOF_MP_COUNT_FOR_VICTORY);
	mask.set(KINDOF_STRUCTURE);
	return (pBuildingCountParm->getInt() >= pPlayer->countObjects(mask, KINDOFMASK_NONE));
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerHasPower */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasPower(Parameter *pPlayerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	Energy* pPlayersEnergy = pPlayer->getEnergy();
	if (!pPlayersEnergy) {
		return false;
	}
	return pPlayersEnergy->hasSufficientPower();
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedReachedWaypointsEnd */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedReachedWaypointsEnd(Parameter *pUnitParm, Parameter* pWaypointPathParm)
{
	Object *theObj = TheScriptEngine->getUnitNamed( pUnitParm->getString() );
	if (!theObj) {
		return false;
	}

	AIUpdateInterface *ai = theObj->getAIUpdateInterface();
	if (!ai)
		return false;

	const Waypoint *targetWay = ai->getCompletedWaypoint();

	if (!targetWay) return false;

	AsciiString	pathName = pWaypointPathParm->getString();

	if (targetWay->getPathLabel1() == pathName) return true;
	if (targetWay->getPathLabel2() == pathName) return true;
	if (targetWay->getPathLabel3() == pathName) return true;

	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamReachedWaypointsEnd */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamReachedWaypointsEnd(Parameter *pTeamParm, Parameter* pWaypointPathParm)
{
	Team *theTeam = TheScriptEngine->getTeamNamed( pTeamParm->getString() );
	if (!theTeam) {
		return false;
	}

	AsciiString	pathName = pWaypointPathParm->getString();
	Bool anyAtEnd = false;
	Bool anyNotAtEnd = false;
	// Note - This returns true if any of the team completed the path.  This is as the current
	// implementation tends to do group pathfinding by default, so we trigger when the leader actually thinks
	// that he has reached the end of the waypoint path.
//int i = 0;
	for (DLINK_ITERATOR<Object> iter = theTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
		Object *pObj = iter.cur();
		if (!pObj) {
			continue;
		}
		AIUpdateInterface *ai = pObj->getAIUpdateInterface();
		if (!ai) continue; // in case there are any rocks or trees in the team :)

		const Waypoint *targetWay = ai->getCompletedWaypoint();

		if (!targetWay) {
			anyNotAtEnd = true;
			continue;
		}
		Bool found = false;
		if (targetWay->getPathLabel1() == pathName) found = true;
		if (targetWay->getPathLabel2() == pathName) found = true;
		if (targetWay->getPathLabel3() == pathName) found = true;
		if (found) {
			anyAtEnd = true;
		} else {
			anyNotAtEnd = true;
		}
	}
	return anyAtEnd;
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedSelected */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedSelected(Condition *pCondition, Parameter *pUnitParm)
{
	if (TheGameEngine->isMultiplayerSession())
	{
		return false;
	}


	Bool anyChanges = false;
	if (pCondition->getCustomData() == 0) anyChanges = true;


	if (TheInGameUI->getFrameSelectionChanged() != pCondition->getCustomFrame()) {
		anyChanges = true; // Selection changed since we cached the value.  jba.
	}
	if (!anyChanges) {
		if (pCondition->getCustomData()==-1) return false;
		if (pCondition->getCustomData()==1) return true;
	}

	Bool isSelected = false;
	const DrawableList *selected = TheInGameUI->getAllSelectedDrawables();

	// loop through all the selected drawables
	Drawable *draw;
	for( DrawableListCIt it = selected->begin(); it != selected->end(); ++it )
	{
		draw = *it;

		if (draw->getObject()->getName() == (pUnitParm->getString())) {
			isSelected = true;
			break;
		}

	}

	pCondition->setCustomData(-1); // false.
	if (isSelected) {
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheInGameUI->getFrameSelectionChanged());
	return isSelected;
}

//-------------------------------------------------------------------------------------------------
/** evaluateVideoHasCompleted */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateVideoHasCompleted(Parameter *pVideoParm)
{
	return TheScriptEngine->isVideoComplete(pVideoParm->getString(), true);
}

//-------------------------------------------------------------------------------------------------
/** evaluateSpeechHasCompleted */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSpeechHasCompleted(Parameter *pSpeechParm)
{
	return TheScriptEngine->isSpeechComplete(pSpeechParm->getString(), true);
}

//-------------------------------------------------------------------------------------------------
/** evaluateAudioHasCompleted */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateAudioHasCompleted(Parameter *pAudioParm)
{
	return TheScriptEngine->isAudioComplete(pAudioParm->getString(), true);
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerSpecialPowerFromUnitTriggered */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerSpecialPowerFromUnitTriggered(Parameter *pPlayerParm, Parameter *pSpecialPowerParm, Parameter* pUnitParm)
{
	Player *pPlayer = playerFromParam(pPlayerParm);
	ObjectID sourceID = INVALID_ID;
	if (pUnitParm)
	{
		Object* pUnit = TheScriptEngine->getUnitNamed(pUnitParm->getString());
		if (!pUnit)
		{
			// we cared about the source object, but it is dead.  No sense checking anymore, since we don't know it's objectID anymore. :P
			return FALSE;
		}
		sourceID = pUnit->getID();
	}

	if (pPlayer)
	{
		return TheScriptEngine->isSpecialPowerTriggered(pPlayer->getPlayerIndex(), pSpecialPowerParm->getString(), TRUE, sourceID);
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerSpecialPowerFromUnitMidway */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerSpecialPowerFromUnitMidway(Parameter *pPlayerParm, Parameter *pSpecialPowerParm, Parameter* pUnitParm)
{
	Player *pPlayer = playerFromParam(pPlayerParm);
	ObjectID sourceID = INVALID_ID;
	if (pUnitParm)
	{
		Object* pUnit = TheScriptEngine->getUnitNamed(pUnitParm->getString());
		if (!pUnit)
		{
			// we cared about the source object, but it is dead.  No sense checking anymore, since we don't know it's objectID anymore. :P
			return FALSE;
		}
		sourceID = pUnit->getID();
	}

	if (pPlayer)
	{
		return TheScriptEngine->isSpecialPowerMidway(pPlayer->getPlayerIndex(), pSpecialPowerParm->getString(), TRUE, sourceID);
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerSpecialPowerFromUnitComplete */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerSpecialPowerFromUnitComplete(Parameter *pPlayerParm, Parameter *pSpecialPowerParm, Parameter* pUnitParm)
{
	Player *pPlayer = playerFromParam(pPlayerParm);
	ObjectID sourceID = INVALID_ID;
	if (pUnitParm)
	{
		Object* pUnit = TheScriptEngine->getUnitNamed(pUnitParm->getString());
		if (!pUnit)
		{
			// we cared about the source object, but it is dead.  No sense checking anymore, since we don't know it's objectID anymore. :P
			return FALSE;
		}
		sourceID = pUnit->getID();
	}

	if (pPlayer)
	{
		return TheScriptEngine->isSpecialPowerComplete(pPlayer->getPlayerIndex(), pSpecialPowerParm->getString(), TRUE, sourceID);
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluateUpgradeFromUnitComplete */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateUpgradeFromUnitComplete(Parameter *pPlayerParm, Parameter *pUpgradeParm, Parameter* pUnitParm)
{
	Player *pPlayer = playerFromParam(pPlayerParm);
	ObjectID sourceID = INVALID_ID;
	if (pUnitParm)
	{
		Object* pUnit = TheScriptEngine->getUnitNamed(pUnitParm->getString());
		if (!pUnit)
		{
			// we cared about the source object, but it is dead.  No sense checking anymore, since we don't know it's objectID anymore. :P
			return FALSE;
		}
		sourceID = pUnit->getID();
	}

	if (pPlayer)
	{
		return TheScriptEngine->isUpgradeComplete(pPlayer->getPlayerIndex(), pUpgradeParm->getString(), TRUE, sourceID);
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluateScienceAcquired */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateScienceAcquired(Parameter *pPlayerParm, Parameter *pScienceParm)
{
	Player *pPlayer = playerFromParam(pPlayerParm);
	if (pPlayer)
	{
		ScienceType science = TheScienceStore->getScienceFromInternalName(pScienceParm->getString());
		if (science == SCIENCE_INVALID)
			return FALSE;
		return TheScriptEngine->isScienceAcquired(pPlayer->getPlayerIndex(), science, TRUE);
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluateCanPurchaseScience */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateCanPurchaseScience(Parameter *pPlayerParm, Parameter *pScienceParm)
{
	Player *pPlayer = playerFromParam(pPlayerParm);
	if (pPlayer)
	{
		ScienceType science = TheScienceStore->getScienceFromInternalName(pScienceParm->getString());
		if (science == SCIENCE_INVALID)
			return FALSE;
		return pPlayer->isCapableOfPurchasingScience(science);
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluateSciencePurchasePoints */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSciencePurchasePoints(Parameter *pPlayerParm, Parameter *pSciencePointParm)
{
	Player *pPlayer = playerFromParam(pPlayerParm);
	if (pPlayer)
	{
		Int pointsNeeded = pSciencePointParm->getInt();
		return pPlayer->getSciencePurchasePoints() >= pointsNeeded;
	}
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedHasFreeContainerSlots */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedHasFreeContainerSlots(Parameter *pUnitParm)
{
	Object* pUnit = TheScriptEngine->getUnitNamed(pUnitParm->getString());
	if (!pUnit) {
		return false;
	}

	ContainModuleInterface *contain = pUnit->getContain();
	if( contain )
	{
		UnsignedInt max = contain->getContainMax();
		UnsignedInt cur = contain->getContainCount();

		if( cur < max )
		{
			return TRUE;
		}
	}
	return FALSE;
}


//-------------------------------------------------------------------------------------------------
/** evaluateNamedEnteredArea */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedEnteredArea(Parameter *pUnitParm, Parameter *pTriggerParm)
{
	Object* pUnit = TheScriptEngine->getUnitNamed(pUnitParm->getString());
	if (!pUnit) {
		return false;
	}

	if (pUnit->isKindOf(KINDOF_INERT)) {
		return false;
	}

	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());

	if (!pTrig) {
		return false;
	}

	return (pUnit->didEnter(pTrig));
}

//-------------------------------------------------------------------------------------------------
/** evaluateNamedExitedArea */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNamedExitedArea(Parameter *pUnitParm, Parameter *pTriggerParm)
{
	Object* pUnit = TheScriptEngine->getUnitNamed(pUnitParm->getString());
	if (!pUnit) {
		return false;
	}

	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());

	if (!pTrig) {
		return false;
	}

	return (pUnit->didExit(pTrig));
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamEnteredAreaEntirely */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamEnteredAreaEntirely(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) {
		return false;
	}

	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());

	if (pTrig) {
		return pTeam->didAllEnter(pTrig, (UnsignedInt)pTypeParm->getInt());
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamEnteredAreaPartially */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamEnteredAreaPartially(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) {
		return false;
	}

	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());

	if (pTrig) {
		return pTeam->didPartialEnter(pTrig, (UnsignedInt)pTypeParm->getInt());
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamExitedAreaEntirely */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamExitedAreaEntirely(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) {
		return false;
	}

	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());

	if (!pTrig) {
		return false;
	}

	return (pTeam->didAllExit(pTrig, (UnsignedInt)pTypeParm->getInt()));
}

//-------------------------------------------------------------------------------------------------
/** evaluateTeamExitedAreaPartially */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamExitedAreaPartially(Parameter *pTeamParm, Parameter *pTriggerParm, Parameter *pTypeParm)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) {
		return false;
	}

	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());

	if (!pTrig) {
		return false;
	}

	return (pTeam->didPartialExit(pTrig, (UnsignedInt)pTypeParm->getInt()));
}

//-------------------------------------------------------------------------------------------------
/** evaluateMultiplayerAlliedVictory */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMultiplayerAlliedVictory()
{
	return TheVictoryConditions->isLocalAlliedVictory();
}

//-------------------------------------------------------------------------------------------------
/** evaluateMultiplayerAlliedDefeat */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMultiplayerAlliedDefeat()
{
	return TheVictoryConditions->isLocalAlliedDefeat();
}

//-------------------------------------------------------------------------------------------------
/** evaluateMultiplayerPlayerDefeat */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMultiplayerPlayerDefeat()
{
	return TheVictoryConditions->isLocalDefeat() && !TheVictoryConditions->isLocalAlliedDefeat();
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerUnitCondition */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerUnitCondition(Condition *pCondition, Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm, Parameter *pUnitTypeParm)
{
	if (pCondition->getCustomData()!=0)
	{
		// We have a cached value.
		if( TheScriptEngine->getFrameObjectCountChanged() == pCondition->getCustomFrame() )
		{
			// object count hasn't changed since we cached.  Use cached value.
			if( pCondition->getCustomData() == 1 )
			{
				return true;
			}
			if( pCondition->getCustomData() == -1 )
			{
				return false;
			}
		}
	}

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer)
	{
		return false;
	}

	ObjectTypesTemp types;
	objectTypesFromParam(pUnitTypeParm, types.m_types);

	std::vector<Int> counts;
	std::vector<const ThingTemplate *> templates;

	Int numObjs = types.m_types->prepForPlayerCounting(templates, counts);
	Int count = 0;

	if (numObjs > 0)
	{
		pPlayer->countObjectsByThingTemplate(numObjs, &(*templates.begin()), false, &(*counts.begin()));
		count = rts::sum(counts);
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN :
			comparison = (count < pCountParm->getInt());
			break;
		case Parameter::LESS_EQUAL :
			comparison = (count <= pCountParm->getInt());
			break;
		case Parameter::EQUAL :
			comparison = (count == pCountParm->getInt());
			break;
		case Parameter::GREATER_EQUAL :
			comparison = (count >= pCountParm->getInt());
			break;
		case Parameter::GREATER :
			comparison = (count > pCountParm->getInt());
			break;
		case Parameter::NOT_EQUAL :
			comparison = (count != pCountParm->getInt());
			break;
		default:
			DEBUG_CRASH(("ScriptConditions::evaluatePlayerUnitCondition: Invalid comparison type. (jkmcd)"));
			break;
	}
	pCondition->setCustomData(-1); // false.
	if (comparison)
	{
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
/** evaluatePlayerHasComparisonPercentPower */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonPercentPower(Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pPercentParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}
	Real powerRatio = pPlayer->getEnergy()->getEnergySupplyRatio();
	Real testRatio = pPercentParm->getInt()/100.0f;
	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN :			return (powerRatio < testRatio);
		case Parameter::LESS_EQUAL :		return (powerRatio <= testRatio);
		case Parameter::EQUAL :					return (powerRatio == testRatio);
		case Parameter::GREATER_EQUAL :	return (powerRatio >= testRatio);
		case Parameter::GREATER :				return (powerRatio > testRatio);
		case Parameter::NOT_EQUAL:			return (powerRatio != testRatio);
	}
	return false;
}
//-------------------------------------------------------------------------------------------------
/** evaluatePlayerHasComparisonValueExcessPower */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonValueExcessPower(Parameter *pPlayerParm, Parameter *pComparisonParm, Parameter *pKWHParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}
	Int desiredKilowattExcess = pKWHParm->getInt();
	Int actualKilowats = pPlayer->getEnergy()->getProduction() - pPlayer->getEnergy()->getConsumption();
	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN :			return (actualKilowats < desiredKilowattExcess);
		case Parameter::LESS_EQUAL :		return (actualKilowats <= desiredKilowattExcess);
		case Parameter::EQUAL :					return (actualKilowats == desiredKilowattExcess);
		case Parameter::GREATER_EQUAL :	return (actualKilowats >= desiredKilowattExcess);
		case Parameter::GREATER :				return (actualKilowats > desiredKilowattExcess);
		case Parameter::NOT_EQUAL:			return (actualKilowats != desiredKilowattExcess);
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
/** evaluateSkirmishSpecialPowerIsReady - does any unit have this special power ready to use? */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishSpecialPowerIsReady(Parameter *pSkirmishPlayerParm, Parameter *pPower)
{
	if (pPower->getInt() == -1) return false;
	if (pPower->getInt()>0 && pPower->getInt()>TheGameLogic->getFrame()) {
		return false;
	}
	Int nextFrame = TheGameLogic->getFrame() + 10*LOGICFRAMES_PER_SECOND;
	const SpecialPowerTemplate *power = TheSpecialPowerStore->findSpecialPowerTemplate(pPower->getString());
	if (power==nullptr) {
		pPower->friend_setInt(-1); // flag as never true.
		return false;
	}
	Bool found = false;
	Player::PlayerTeamList::const_iterator it;
	Player *pPlayer = playerFromParam(pSkirmishPlayerParm);
	if (pPlayer==nullptr)
		return false;

	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) continue;
			for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object *pObj = iter.cur();
				if (!pObj) continue;
				if( pObj->getStatusBits().test( OBJECT_STATUS_UNDER_CONSTRUCTION ) || pObj->isDisabled() )
				{
					continue; // can't fire if under construction or disabled.
				}
				SpecialPowerModuleInterface *mod = pObj->getSpecialPowerModule(power);
				if (mod)
				{
					if (!TheSpecialPowerStore->canUseSpecialPower(pObj, power)) {
						continue;
					}
					found = true;
					if (mod->isReady()) return true;
					if (mod->getReadyFrame()<nextFrame) nextFrame = mod->getReadyFrame();
				}

			}
		}
	}
	pPower->friend_setInt(nextFrame);
	return false;
}


//-------------------------------------------------------------------------------------------------
/** evaluatePlayerDestroyedNOrMoreBuildings */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerDestroyedNOrMoreBuildings(Parameter *pPlayerParm, Parameter *pNumParm, Parameter *pOpponentParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	Player* pOpponent = playerFromParam(pOpponentParm);
//Int N = pNumParm->getInt();
	if (!pPlayer || !pOpponent) {
		return false;
	}

	/// @todo CLH implement me!
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
/** evaluateUnitHasEmptied */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateUnitHasEmptied(Parameter *pUnitParm)
{
	Object *object = TheScriptEngine->getUnitNamed(pUnitParm->getString());
	if (!object) {
		return false;
	}

	// have we checked this one before?
	TransportStatus *stats = s_transportStatuses;
	while (stats) {
		if (stats->m_objID == object->getID()) {
			break;
		}

		stats = stats->m_nextStatus;
	}

	ContainModuleInterface *cmi = object->getContain();
	Int numPeeps = cmi ? cmi->getContainCount() : 0;

	UnsignedInt frameNum = TheGameLogic->getFrame();


	if (stats == nullptr)
	{
		TransportStatus *transportStatus = newInstance(TransportStatus);
		transportStatus->m_objID = object->getID();
		transportStatus->m_frameNumber = frameNum;
		transportStatus->m_unitCount = numPeeps;
		transportStatus->m_nextStatus = s_transportStatuses;
		s_transportStatuses = transportStatus;
		return false;
	}

	if (stats->m_frameNumber == frameNum - 1) {
		if (stats->m_unitCount > 0 && numPeeps == 0) {
			// don't actually update the info on this round, because we want to make sure that
			// multiple calls to this in the same frame actually work.
			return true;
		}
	}

	// perform the update.
	stats->m_frameNumber = frameNum;
	stats->m_unitCount = numPeeps;
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamIsContained(Parameter *pTeamParm, Bool allContained)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) {
		return false;
	}

	Bool anyConsidered = FALSE;
	for (DLINK_ITERATOR<Object> iter = pTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
		Object *obj = iter.cur();
		if (!obj) {
			continue;
		}

		Bool isContained = (obj->getContainedBy() != nullptr);
		if (!isContained) {
			// we could still be exiting, in which case we should pretend like we are contained.

			AIUpdateInterface *ai = obj->getAIUpdateInterface();
			if (ai) {
				isContained = (isContained && (ai->getCurrentStateID() == AI_EXIT));
			}
		}

		if (isContained) {
			if (!allContained) {
				return TRUE;
			}
		} else {
			if (allContained) {
				return FALSE;
			}
		}

		anyConsidered = TRUE;
	}

	if (anyConsidered) {
		return allContained;
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateUnitHasObjectStatus(Parameter *pUnitParm, Parameter *pObjectStatus)
{
	Object *object = TheScriptEngine->getUnitNamed(pUnitParm->getString());
	if (!object) {
		return false;
	}

	return( object->getStatusBits().testForAny( pObjectStatus->getStatus() ) );
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamHasObjectStatus(Parameter *pTeamParm, Parameter *pObjectStatus, Bool entireTeam)
{
	Team *theTeam = TheScriptEngine->getTeamNamed( pTeamParm->getString() );
	if (!theTeam) {
		return false;
	}

	for (DLINK_ITERATOR<Object> iter = theTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
		Object *pObj = iter.cur();
		if (!pObj) {
			return false;
		}

		ObjectStatusMaskType objStatus = pObjectStatus->getStatus();
		Bool currObjHasStatus = pObj->getStatusBits().testForAny( objStatus );

		if( entireTeam && !currObjHasStatus )
		{
			return false;
		}
		else if( !entireTeam && currObjHasStatus )
		{
			return true;
		}

	}

	if (entireTeam) {
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
// @todo: PERF_EVALUATE Get a perf timer on this. Should we adjust this function so that it runs like
// evaluatePlayerHasUnitKindInArea
// ?
Bool ScriptConditions::evaluateSkirmishValueInArea(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pComparisonParm, Parameter *pMoneyParm, Parameter *pTriggerParm)
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return false;
	}

	AsciiString triggerName = pTriggerParm->getString();
	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());

	if (!pTrig) {
		return false;
	}

	Player::PlayerTeamList::const_iterator it;
	Bool anyChanges = false;
	if (pCondition->getCustomData() == 0) anyChanges = true;


	for (it = player->getPlayerTeams()->begin(); it != player->getPlayerTeams()->end(); ++it) {
		if (anyChanges) break;
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			if (anyChanges) break;
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			if (team->didEnterOrExit()) {
				anyChanges = true;
			}
		}
	}
	if (TheScriptEngine->getFrameObjectCountChanged() != pCondition->getCustomFrame()) {
		anyChanges = true; // Objects were added/deleted since we cached, so count could have changed.  jba.
	}
	if (!anyChanges) {
		if (pCondition->getCustomData()==-1) return false;
		if (pCondition->getCustomData()==1) return true;
	}
	Int totalCost = 0;
	for (it = player->getPlayerTeams()->begin(); it != player->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object *pObj = iter.cur();
				if (!pObj) {
					continue;
				}
				if (!pObj->isKindOf(KINDOF_INERT) && pObj->isInside(pTrig)) {
					if (!pObj->isEffectivelyDead()) {
						const ThingTemplate *tt = pObj->getTemplate();
						if (!tt) {
							continue;
						}
						totalCost += tt->friend_getBuildCost();
					}
				}
			}
		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN :			comparison = (totalCost < pMoneyParm->getInt()); break;
		case Parameter::LESS_EQUAL :		comparison = (totalCost <= pMoneyParm->getInt()); break;
		case Parameter::EQUAL :					comparison = (totalCost == pMoneyParm->getInt()); break;
		case Parameter::GREATER_EQUAL :	comparison = (totalCost >= pMoneyParm->getInt()); break;
		case Parameter::GREATER :				comparison = (totalCost > pMoneyParm->getInt()); break;
		case Parameter::NOT_EQUAL :			comparison = (totalCost != pMoneyParm->getInt()); break;
		default:
			DEBUG_CRASH(("ScriptConditions::evaluateSkirmishValueInArea: Invalid comparison type. (jkmcd)"));
			break;
	}
	pCondition->setCustomData(-1); // false.
	if (comparison) {
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishPlayerIsFaction(Parameter *pSkirmishPlayerParm, Parameter *pFactionParm)
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return false;
	}

	return (player->getSide() == pFactionParm->getString());
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishSuppliesWithinDistancePerimeter(Parameter *pSkirmishPlayerParm, Parameter *pDistanceParm, Parameter *pLocationParm, Parameter *pValueParm)
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return false;
	}

	PolygonTrigger *trigger = TheScriptEngine->getQualifiedTriggerAreaByName(pLocationParm->getString());
	if (!trigger) {
		return false;
	}

	Coord3D center;
	trigger->getCenterPoint(&center);
	Real distance = trigger->getRadius() + pDistanceParm->getReal();

	Real compareToValue = pValueParm->getReal();

	PartitionFilterAcceptByKindOf f1(MAKE_KINDOF_MASK(KINDOF_STRUCTURE), KINDOFMASK_NONE);
	PartitionFilterPlayerAffiliation f2(player, ALLOW_NEUTRAL, true);
	PartitionFilterOnMap filterMapStatus;

	PartitionFilter *filters[] = { &f1, &f2, &filterMapStatus, nullptr };

	SimpleObjectIterator *iter = ThePartitionManager->iterateObjectsInRange(&center, distance, FROM_CENTER_2D, filters, ITER_FASTEST);
	MemoryPoolObjectHolder hold(iter);

	Real maxValue = 0;
	for (Object *them = iter->first(); them; them = iter->next()) {
		static const NameKeyType key_warehouseUpdate = NAMEKEY("SupplyWarehouseDockUpdate");
		SupplyWarehouseDockUpdate *warehouseModule = (SupplyWarehouseDockUpdate*) them->findUpdateModule( key_warehouseUpdate );
		if (!warehouseModule) {
			continue;
		}

		Real value = player->getSupplyBoxValue() * warehouseModule->getBoxesStored();
		if (value > maxValue) {
			maxValue = value;
		}
	}

	return maxValue > compareToValue;
}

//-------------------------------------------------------------------------------------------------
// @todo: PERF_EVALUATE PERF_WARNING
// If this is called multiple times per frame, the cost could add up. This will infrequently (read:
// never) change, so it shouldn't be called very often. jkmcd
Bool ScriptConditions::evaluateSkirmishPlayerTechBuildingWithinDistancePerimeter(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pDistanceParm, Parameter *pLocationParm)
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return false;
	}
	// If we have a chached value, return it. [8/8/2003]
	if (pCondition->getCustomData()==1) return true;
	if (pCondition->getCustomData()==-1) return false;

	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pLocationParm->getString());
	if (!pTrig) {
		return false;
	}

	Coord3D center;
	pTrig->getCenterPoint(&center);
	Real radius = pTrig->getRadius() + pDistanceParm->getReal();

	PartitionFilterAcceptByKindOf f1(MAKE_KINDOF_MASK(KINDOF_TECH_BUILDING), KINDOFMASK_NONE);
	PartitionFilterPlayerAffiliation f2(player, ALLOW_ALLIES, false);
	PartitionFilterPlayer f3(player, false);	// Don't find your own units, as our affiliation to self is neutral.
	PartitionFilterOnMap filterMapStatus;


	PartitionFilter *filters[] = { &f1, &f2, &f3, &filterMapStatus, nullptr };

	Bool comparison = ThePartitionManager->getClosestObject(&center, radius, FROM_CENTER_2D, filters) != nullptr;
	pCondition->setCustomData(-1); // false.
	if (comparison) {
		pCondition->setCustomData(1); // true.
	}
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishCommandButtonIsReady( Parameter * /* pSkirmishPlayerParm */, Parameter *pTeamParm, Parameter *pCommandButtonParm, Bool allReady )
{
	// In this one case, the pSkirmishPlayerParm isn't used.
	Team *theTeam = TheScriptEngine->getTeamNamed( pTeamParm->getString() );
	if (!theTeam) {
		return false;
	}

	const CommandButton *commandButton = TheControlBar->findCommandButton( pCommandButtonParm->getString() );
	if( !commandButton ) {
		return false;
	}

	for (DLINK_ITERATOR<Object> iter = theTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
		Object *pObj = iter.cur();
		if (commandButton->getSpecialPowerTemplate()) {
			if( !pObj->hasSpecialPower( commandButton->getSpecialPowerTemplate()->getSpecialPowerType() ) ) {
				continue;
			}
		} else if (!commandButton->getUpgradeTemplate()) {
			continue;
		}

		if (commandButton->isReady(pObj)) {
			if (!allReady) {
				return true;
			}
		} else {
			if (allReady) {
				return false;
			}
		}
	}

	return allReady;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishUnownedFactionUnitComparison( Parameter * /*pSkirmishPlayerParm*/, Parameter *pComparisonParm, Parameter *pCountParm )
{
	Player *player = ThePlayerList->getNeutralPlayer();
	if (!player) {
		return FALSE;
	}

	// Note: This looks slow, but in practice it shouldn't be. The neutral player shouldn't really ever
	// have more than one team.
	Int numFactionUnits = 0;
	Player::PlayerTeamList::const_iterator it;
	for (it = player->getPlayerTeams()->begin(); it != player->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance()) {
				Object *obj = objIter.cur();
				if (!obj) {
					continue;
				}

				if( obj->isDisabledByType( DISABLED_UNMANNED ) )
				{
					++numFactionUnits;
				}
			}
		}
	}

	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN			:	return numFactionUnits < pCountParm->getInt();	break;
		case Parameter::LESS_EQUAL		:	return numFactionUnits <= pCountParm->getInt(); break;
		case Parameter::EQUAL					:	return numFactionUnits == pCountParm->getInt(); break;
		case Parameter::GREATER_EQUAL :	return numFactionUnits >= pCountParm->getInt(); break;
		case Parameter::GREATER				:	return numFactionUnits > pCountParm->getInt();	break;
		case Parameter::NOT_EQUAL			:	return numFactionUnits != pCountParm->getInt();	break;
	}

	DEBUG_CRASH(("ScriptConditions::evaluateSkirmishUnownedFactionUnitComparison: Invalid comparison type. (jkmcd)"));
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishPlayerHasPrereqsToBuild( Parameter *pSkirmishPlayerParm, Parameter *pObjectTypeParm )
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return FALSE;
	}

	ObjectTypesTemp types;
	objectTypesFromParam(pObjectTypeParm, types.m_types);

	return types.m_types->canBuildAny(player);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishPlayerHasComparisonGarrisoned(Parameter *pSkirmishPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm )
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return FALSE;
	}

	// Note: This looks slow, and probably is.
	// @todo: PERF_EVALUATE
	Int numGarrisonedBuildings = 0;
	Player::PlayerTeamList::const_iterator it;
	for (it = player->getPlayerTeams()->begin(); it != player->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance()) {
				Object *obj = objIter.cur();
				if (!obj) {
					continue;
				}

				ContainModuleInterface *cmi = obj->getContain();
				if (!cmi) {
					continue;
				}

				if (cmi->isGarrisonable() && cmi->getContainCount() > 0) {
					++numGarrisonedBuildings;
				}
			}
		}
	}

	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN			:	return numGarrisonedBuildings < pCountParm->getInt();	break;
		case Parameter::LESS_EQUAL		:	return numGarrisonedBuildings <= pCountParm->getInt(); break;
		case Parameter::EQUAL					:	return numGarrisonedBuildings == pCountParm->getInt(); break;
		case Parameter::GREATER_EQUAL :	return numGarrisonedBuildings >= pCountParm->getInt(); break;
		case Parameter::GREATER				:	return numGarrisonedBuildings > pCountParm->getInt();	break;
		case Parameter::NOT_EQUAL			:	return numGarrisonedBuildings != pCountParm->getInt();	break;
	}

	DEBUG_CRASH(("ScriptConditions::evaluateSkirmishPlayerHasComparisonGarrisoned: Invalid comparison type. (jkmcd)"));
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishPlayerHasComparisonCapturedUnits(Parameter *pSkirmishPlayerParm, Parameter *pComparisonParm, Parameter *pCountParm )
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return FALSE;
	}

	// Note: This looks slow, and probably is.
	// @todo: PERF_EVALUATE
	Int numCapturedUnits = 0;
	Player::PlayerTeamList::const_iterator it;
	for (it = player->getPlayerTeams()->begin(); it != player->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance()) {
				Object *obj = objIter.cur();
				if (!obj) {
					continue;
				}

				if (obj->isCaptured()) {
					++numCapturedUnits;
				}
			}
		}
	}

	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN			:	return numCapturedUnits < pCountParm->getInt();	break;
		case Parameter::LESS_EQUAL		:	return numCapturedUnits <= pCountParm->getInt(); break;
		case Parameter::EQUAL					:	return numCapturedUnits == pCountParm->getInt(); break;
		case Parameter::GREATER_EQUAL :	return numCapturedUnits >= pCountParm->getInt(); break;
		case Parameter::GREATER				:	return numCapturedUnits > pCountParm->getInt();	break;
		case Parameter::NOT_EQUAL			:	return numCapturedUnits != pCountParm->getInt();	break;
	}

	DEBUG_CRASH(("ScriptConditions::evaluateSkirmishPlayerHasComparisonCapturedUnits: Invalid comparison type. (jkmcd)"));
	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishNamedAreaExists(Parameter *, Parameter *pTriggerParm)
{
	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	return (pTrig != nullptr);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishPlayerHasUnitsInArea(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pTriggerParm )
{
	AsciiString triggerName = pTriggerParm->getString();
	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (pTrig == nullptr) return false;

	Player* pPlayer = playerFromParam(pSkirmishPlayerParm);
	if (!pPlayer) {
		return false;
	}
	Player::PlayerTeamList::const_iterator it;
	Bool anyChanges = false;


	if (pCondition->getCustomData() == 0) anyChanges = true;

	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		if (anyChanges) break;
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			if (anyChanges) break;
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			if (team->didEnterOrExit()) {
				anyChanges = true;
			}
		}
	}

	if (TheScriptEngine->getFrameObjectCountChanged() > pCondition->getCustomFrame()) {
		anyChanges = true; // Objects were added/deleted last frame, so count could have changed.  jba.
	}

	if (!anyChanges) {
		if (pCondition->getCustomData()==-1) return false;
		if (pCondition->getCustomData()==1) return true;
	}

	Int count = 0;
	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}
			for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object *pObj = iter.cur();
				if (!pObj) {
					continue;
				}

				if (pObj->isInside(pTrig)) {

					//
					// dead objects will not be considered.
					//
					if (!(pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT) || pObj->isKindOf(KINDOF_PROJECTILE)) ) {
						count++;
					}
				}
			}
		}
	}

	Bool comparison = count > 0;
	pCondition->setCustomData(-1); // false.
	if (comparison) {
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishSupplySourceSafe(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pMinSupplyAmount )
{
	// Trigger every 2*LOGICFRAMES_PER_SECOND. jba.
	Bool anyChanges = (TheGameLogic->getFrame() > pCondition->getCustomFrame());
	if (!anyChanges) {
		if (pCondition->getCustomData()==-1) return false;
		if (pCondition->getCustomData()==1) return true;
	}
	pCondition->setCustomFrame(TheGameLogic->getFrame()+2*LOGICFRAMES_PER_SECOND);
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return FALSE;
	}
	Bool isSafe = player->isSupplySourceSafe(pMinSupplyAmount->getInt());
	pCondition->setCustomData(-1); // false.
	if (isSafe) {
		pCondition->setCustomData(1); // true.
	}
	return isSafe;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishSupplySourceAttacked(Parameter *pSkirmishPlayerParm)
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return FALSE;
	}
	return player->isSupplySourceAttacked();
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishStartPosition(Parameter *pSkirmishPlayerParm, Parameter *pStartNdx)
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return FALSE;
	}
	Int ndx = pStartNdx->getInt()-1;  // externally 1, 2, 3, internally 0, 1, 2.
	Int startNdx = player->getMpStartIndex();
	return ndx == startNdx;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishPlayerHasBeenAttackedByPlayer(Parameter *pSkirmishPlayerParm, Parameter *pAttackedByParm )
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return FALSE;
	}

	Player *srcPlayer = playerFromParam(pAttackedByParm);
	if (!srcPlayer ) {
		return FALSE;
	}

	return player->getAttackedBy(srcPlayer->getPlayerIndex());
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishPlayerIsOutsideArea(Condition *pCondition, Parameter *pSkirmishPlayerParm, Parameter *pTriggerParm )
{
	// Even though these will be prechecked in the other function, we want to preflight here as well.
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return FALSE;
	}

	PolygonTrigger *pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pTrig) {
		return FALSE;
	}

	return !evaluateSkirmishPlayerHasUnitsInArea(pCondition, pSkirmishPlayerParm, pTriggerParm);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishPlayerHasDiscoveredPlayer(Parameter *pSkirmishPlayerParm, Parameter *pDiscoveredByParm )
{
	Player *player = playerFromParam(pSkirmishPlayerParm);
	if (!player) {
		return FALSE;
	}

	Player *discoveredBy = playerFromParam(pDiscoveredByParm);
	if (!discoveredBy) {
		return FALSE;
	}

	Int discoveredByIndex = discoveredBy->getPlayerIndex();

	Player::PlayerTeamList::const_iterator it;
	for (it = player->getPlayerTeams()->begin(); it != player->getPlayerTeams()->end(); ++it) {
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team *team = iter.cur();
			if (!team) {
				continue;
			}

			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance()) {
				Object *obj = objIter.cur();
				if (!obj) {
					continue;
				}

				ObjectShroudStatus shroudStatus = obj->getShroudedStatus(discoveredByIndex);
				if (shroudStatus == OBJECTSHROUD_CLEAR || shroudStatus == OBJECTSHROUD_PARTIAL_CLEAR) {
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMusicHasCompleted(Parameter *pMusicParm, Parameter *pIntParm)
{
	AsciiString str = pMusicParm->getString();
	return TheAudio->hasMusicTrackCompleted(str, pIntParm->getInt());
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerLostObjectType(Parameter *pPlayerParm, Parameter *pTypeParm)
{
	Player *player = playerFromParam(pPlayerParm);
	if (!player) {
		return FALSE;
	}

	ObjectTypesTemp objs;
	objectTypesFromParam(pTypeParm, objs.m_types);

	std::vector<Int> counts;
	std::vector<const ThingTemplate *> templates;

	Int numTemplates = objs.m_types->prepForPlayerCounting(templates, counts);
	if (numTemplates > 0) {
		player->countObjectsByThingTemplate(numTemplates, &(*templates.begin()), true, &(*counts.begin()));
	} else {
		return FALSE;
	}

	Int sumOfObjs = rts::sum(counts);
	Int currentCount = TheScriptEngine->getObjectCount(player->getPlayerIndex(), pTypeParm->getString());

	if (sumOfObjs != currentCount) {
		TheScriptEngine->setObjectCount(player->getPlayerIndex(), pTypeParm->getString(), sumOfObjs);
	}

	return (sumOfObjs < currentCount);
}





//-------------------------------------------------------------------------------------------------
//------------------------------ @CLP_AI SCRIPT CONDITION ADDITIONS -------------------------------
//-------------------------------------------------------------------------------------------------


Bool ScriptConditions::evaluatePlayerRelation(const AsciiString& playerSrcName, Int relationType, const AsciiString& playerDstName)
{
	Player* pPlayerSrcName = TheScriptEngine->getPlayerFromAsciiString(playerSrcName);
	Player* pPlayerDstName = TheScriptEngine->getPlayerFromAsciiString(playerDstName);

  if (!pPlayerSrcName || !pPlayerDstName) {
		return false;
	}
  //@-TanSo-: use the default team to get an indirect relationship between players
	Team* pTeamDst = pPlayerDstName->getDefaultTeam();

	return pPlayerSrcName->getRelationship(pTeamDst) == relationType;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateEmptySpot(Parameter* pStartNdx)
{
	Int ndx = pStartNdx->getInt() - 1;
	if (ndx >= 0)
	{
		Int pPlayerCount = ThePlayerList->getPlayerCount();
    // @-TanSo-: iterate through all players
		// i=2 because player 0 is "neutral" & player 1 is "PlyrCivilian".
		// playerCount-1 to skip any possible "observer" player at the end.
		for (int i = 2; i < pPlayerCount - 1; i++)
		{
			Player* pPlayers = ThePlayerList->getNthPlayer(i);
			if (pPlayers->getMpStartIndex() == ndx)
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNeighbouringSpot(Parameter* pPlayerParm, Parameter* pStartNdx)
{
  Player* player = playerFromParam(pPlayerParm);
	if (!player || pStartNdx->getInt() < 1 || pStartNdx->getInt() == player->getMpStartIndex() + 1) { return false; }

  //format the waypoint name for this player's start position
	AsciiString pSpot;
  pSpot.format("Player_%d_Start", player->getMpStartIndex() + 1);

  Waypoint* pWay = TheTerrainLogic->getWaypointByName(pSpot);
	if (!pWay) { return false; }

	//iterate through all player start waypoints to find the closest one
	Coord3D pCoords = *pWay->getLocation();
	Real minDist = FLT_MAX;

	for (Waypoint* iWay = TheTerrainLogic->getFirstWaypoint(); iWay; iWay = iWay->getNext())
	{
		const AsciiString& name = iWay->getName();
		if (name.startsWith("Player_") && name.endsWith("_Start"))
		{
			if (iWay != pWay)
			{
				Coord3D cCoords = *iWay->getLocation();
				Real dx = pCoords.x - cCoords.x;
				Real dy = pCoords.y - cCoords.y;
				Real dist = sqrtf(dx * dx + dy * dy);

				if (dist < minDist) { minDist = dist; }
			}
		}
	}
  //@-TanSo-: if the closest distance still is FLT_MAX, then this is no skirmish map.
	if (minDist == FLT_MAX) { return false; }

	//@-TanSo-: consider neighbouring if within 1.5 times the closest distance.
	minDist *= 1.5f;

	//@-TanSo-: create a Waypoint for the start position to check against
	AsciiString targetName;
	targetName.format("Player_%d_Start", pStartNdx->getInt());
	Waypoint* target = TheTerrainLogic->getWaypointByName(targetName);
	if (!target) { return false; }

	Coord3D tCoords = *target->getLocation();
	Real cX = pCoords.x - tCoords.x;
	Real cY = pCoords.y - tCoords.y;
	Real targetDist = sqrtf(cX * cX + cY * cY);

	return targetDist <= minDist;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateNeighbouringSpotsEmpty(Parameter* pPlayerParm, Parameter* pComparisonParm, Int pCountParm)
{
  Player* player = playerFromParam(pPlayerParm);
	Int playerCount = ThePlayerList->getPlayerCount();
  if (!player || !playerCount) { return false; }

	AsciiString pSpot;
	pSpot.format("Player_%d_Start", player->getMpStartIndex() + 1);
  Waypoint* pWay = TheTerrainLogic->getWaypointByName(pSpot);
  if (!pWay) { return false; }


  Coord3D pCoords = *pWay->getLocation();
  Real minDist = FLT_MAX;
	//iterate through all player start waypoints to find the closest one
	for (Waypoint* iWay = TheTerrainLogic->getFirstWaypoint(); iWay; iWay = iWay->getNext())
	{
		const AsciiString& name = iWay->getName();
		if (name.startsWith("Player_") && name.endsWith("_Start"))
		{
			if (iWay != pWay)
			{
				Coord3D cCoords = *iWay->getLocation();
				Real dx = pCoords.x - cCoords.x;
				Real dy = pCoords.y - cCoords.y;
				Real dist = sqrtf(dx * dx + dy * dy);
				if (dist < minDist)
				{
					minDist = dist;
				}
			}
		}
  }

	std::vector<Waypoint*> neighbouringWaypoints;

  //@-TanSo-: if the closest distance still is FLT_MAX, then this is no skirmish map.
  if (minDist == FLT_MAX) { return false; }

  //@-TanSo-: consider neighbouring if within 1.5 times the closest distance.
  minDist *= 1.5f;

  //@-TanSo-: find all neighbouring waypoints
  for (Waypoint* iWay = TheTerrainLogic->getFirstWaypoint(); iWay; iWay = iWay->getNext())
	{
		const AsciiString& name = iWay->getName();
		if (name.startsWith("Player_") && name.endsWith("_Start"))
		{
			if (iWay != pWay)
			{
				Coord3D cCoords = *iWay->getLocation();
				Real dx = pCoords.x - cCoords.x;
				Real dy = pCoords.y - cCoords.y;
				Real dist = sqrtf(dx * dx + dy * dy);
				if (dist <= minDist)
				{
					neighbouringWaypoints.push_back(iWay);
				}
			}
		}
  }

  // @-TanSo-: Make sure that the newly found neighbouring starting points are *indeed* empty.
	// i=2 because player 0 is "neutral" & player 1 is "PlyrCivilian".
  // playerCount-1 to skip any possible "observer" player at the end.
	for (int i = 2; i < playerCount - 1; i++) {
		Int pIndex = ThePlayerList->getNthPlayer(i)->getMpStartIndex() + 1;
		AsciiString tName;
		tName.format("Player_%d_Start", pIndex);

		for (int j = neighbouringWaypoints.size() - 1; j >= 0; j--) {
			AsciiString sName = neighbouringWaypoints[j]->getName();
			if (sName == tName) {
				neighbouringWaypoints.erase(neighbouringWaypoints.begin() + j);
			}
		}
	}

  //@-TanSo-: compare the new size of the neighbouring EMPTY waypoints with the given count
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:			return neighbouringWaypoints.size() < pCountParm;
	case Parameter::LESS_EQUAL:			return neighbouringWaypoints.size() <= pCountParm;
	case Parameter::EQUAL:					return neighbouringWaypoints.size() == pCountParm;
	case Parameter::GREATER_EQUAL:	return neighbouringWaypoints.size() >= pCountParm;
	case Parameter::GREATER:				return neighbouringWaypoints.size() > pCountParm;
	case Parameter::NOT_EQUAL:			return neighbouringWaypoints.size() != pCountParm;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------

Bool ScriptConditions::evaluateNeighbouringSpotsRelation(Parameter* pPlayerParm, Parameter* pComparisonParm, Int pCount, Int relationType)
{
	Player* player = playerFromParam(pPlayerParm);
	Int playerCount = ThePlayerList->getPlayerCount();
	if (!player || !playerCount) { return false; }

	AsciiString pSpot;
	pSpot.format("Player_%d_Start", player->getMpStartIndex() + 1);
	Waypoint* pWay = TheTerrainLogic->getWaypointByName(pSpot);
	if (!pWay) { return false; }


	Coord3D pCoords = *pWay->getLocation();
	Real minDist = FLT_MAX;
	//@-TanSo-: iterate through all player start waypoints to find the closest one
	for (Waypoint* iWay = TheTerrainLogic->getFirstWaypoint(); iWay; iWay = iWay->getNext())
	{
		const AsciiString& name = iWay->getName();
		if (name.startsWith("Player_") && name.endsWith("_Start"))
		{
			if (iWay != pWay)
			{
				Coord3D cCoords = *iWay->getLocation();
				Real dx = pCoords.x - cCoords.x;
				Real dy = pCoords.y - cCoords.y;
				Real dist = sqrtf(dx * dx + dy * dy);
				if (dist < minDist)
				{
					minDist = dist;
				}
			}
		}
	}

	int count = 0;

	for (int i = 2; i < playerCount - 1; i++)
	{
		Player* other = ThePlayerList->getNthPlayer(i);
		if (!other) continue;

		Int startIndex = other->getMpStartIndex();
		if (startIndex < 0) continue;

		AsciiString name;
		name.format("Player_%d_Start", startIndex + 1);

		Waypoint* w = TheTerrainLogic->getWaypointByName(name);
		if (!w || w == pWay) continue;

		Coord3D c = *w->getLocation();
		Real dx = pCoords.x - c.x;
		Real dy = pCoords.y - c.y;
		Real dist = sqrtf(dx * dx + dy * dy);

		if (dist > minDist)
			continue;

		if (other->getRelationship(player->getDefaultTeam()) == relationType)
			count++;
	}

	//compare the size with the given count
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:			return count < pCount;
	case Parameter::LESS_EQUAL:			return count <= pCount;
	case Parameter::EQUAL:					return count == pCount;
	case Parameter::GREATER_EQUAL:	return count >= pCount;
	case Parameter::GREATER:				return count > pCount;
	case Parameter::NOT_EQUAL:			return count != pCount;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateStartingCash(Parameter* pComparisonParm, Int pAmount)
{
	//@-TanSo-: ThePlayerList->getPlayerCount() - 1 = Last (Dummy) Player. Also receives the starting cash value
	Player* player = ThePlayerList->getNthPlayer(ThePlayerList->getPlayerCount() - 1);
  if (!player) { return false; }
	Int startingCash = player->getMoney()->countMoney();

	switch (pComparisonParm->getInt())
	{
    case Parameter::LESS_THAN:			return startingCash < pAmount;
    case Parameter::LESS_EQUAL:			return startingCash <= pAmount;
    case Parameter::EQUAL:					return startingCash == pAmount;
    case Parameter::GREATER_EQUAL:	return startingCash >= pAmount;
    case Parameter::GREATER:				return startingCash > pAmount;
    case Parameter::NOT_EQUAL:			return startingCash != pAmount;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateClosestRelationUnitToMySpawn(Int relationType, Parameter* pPlayerParm, Parameter* pComparisonParm, Real pDist)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	AsciiString pSpot;
	pSpot.format("Player_%d_Start", pPlayer->getMpStartIndex() + 1);
	Waypoint* pWay = TheTerrainLogic->getWaypointByName(pSpot);
	if (!pWay) return false;

	Real minDist = FLT_MAX;
	Player::PlayerTeamList::const_iterator it;

	for (int i = 2; i < ThePlayerList->getPlayerCount() - 1; i++){
		if (ThePlayerList->getNthPlayer(i)->getRelationship(pPlayer->getDefaultTeam()) == relationType && ThePlayerList->getNthPlayer(i) != pPlayer) {
			for (it = ThePlayerList->getNthPlayer(i)->getPlayerTeams()->begin(); it != ThePlayerList->getNthPlayer(i)->getPlayerTeams()->end(); ++it)
			{
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) continue;

					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj) continue;

						Coord3D oCoords = *pObj->getPosition();
						Real dx = pWay->getLocation()->x - oCoords.x;
						Real dy = pWay->getLocation()->y - oCoords.y;
						Real dist = sqrtf(dx * dx + dy * dy);

						if (dist < minDist) { minDist = dist; }

					}
				}
			}
		}
	}
	if (minDist == FLT_MAX)	return false;

	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:			return minDist < pDist;
	case Parameter::LESS_EQUAL:			return minDist <= pDist;
	case Parameter::EQUAL:					return minDist == pDist;
	case Parameter::GREATER_EQUAL:	return minDist >= pDist;
	case Parameter::GREATER:				return minDist > pDist;
	case Parameter::NOT_EQUAL:			return minDist != pDist;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateHunted(Parameter* pPlayerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	Player::PlayerTeamList::const_iterator it;
	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team* team = iter.cur();
			if (!team) continue;

			for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object* pObj = iter.cur();
				if (!pObj) continue;

				const ThingTemplate* objTmpl = pObj->getTemplate();
				if (!objTmpl) continue;

				if (pObj->isKindOf(KINDOF_COMMANDCENTER) || pObj->isKindOf(KINDOF_DOZER)) return false; // GLA Workers are also Dozers!

				const ThingTemplate* GLAStashTemplate = TheThingFactory->findTemplate("GLASupplyStash");
				const ThingTemplate* ToxStashTemplate = TheThingFactory->findTemplate("Chem_GLASupplyStash");
				const ThingTemplate* DemoStashTemplate = TheThingFactory->findTemplate("Demo_GLASupplyStash");
				const ThingTemplate* SlthStashTemplate = TheThingFactory->findTemplate("Slth_GLASupplyStash");

				if (objTmpl == GLAStashTemplate		||
						objTmpl == ToxStashTemplate		||
						objTmpl == DemoStashTemplate	||
						objTmpl == SlthStashTemplate)
					return false;
			}
		}
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerLostTypeInArea(Parameter* pPlayerParm, Parameter* pObjectType, Parameter* pArea)
{
	// --- Trigger / Area ---
	PolygonTrigger* pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pArea->getString());
	if (!pTrig) { return FALSE; }

	// --- Player ---
	Player* player = playerFromParam(pPlayerParm);
	if (!player) { return FALSE;}

	// --- ObjectTypes ---
	ObjectTypesTemp objs;
	objectTypesFromParam(pObjectType, objs.m_types);

	// --- Prepare templates ---
	std::vector<Int> counts;
	std::vector<const ThingTemplate*> templates;

	Int numTemplates = objs.m_types->prepForPlayerCounting(templates, counts);
	if (numTemplates <= 0) { return FALSE; }

	// --- Manual counting in AREA ---
	// counts[] initializes already with 0
	Player::PlayerTeamList::const_iterator it;
	for (it = player->getPlayerTeams()->begin(); it != player->getPlayerTeams()->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> teamIter = (*it)->iterate_TeamInstanceList(); !teamIter.done(); teamIter.advance())
		{
			Team* team = teamIter.cur();
			if (!team) continue;

			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
			{
				Object* pObj = objIter.cur();
				if (!pObj) continue;

				// Area-Check
				if (!pObj->isInside(pTrig)) {
					continue;
				}

				// Dead-Filter (identical to HasUnitTypeInArea)
				if ((pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) && !pObj->isKindOf(KINDOF_CRATE)) {
					continue;
				}

				// Template-Match
				const ThingTemplate* tmpl = pObj->getTemplate();
				for (Int i = 0; i < numTemplates; ++i) {
					if (tmpl == templates[i]) {
						counts[i]++;
						break;
					}
				}
			}
		}
	}

	// --- Sum ---
	Int sumOfObjs = rts::sum(counts);

	// --- ScriptEngine-State ---
	//IMPORTANT: Area has to be in the key
	AsciiString key;
	key.format("%s_AREA_%s", pObjectType->getString(), pArea->getString());

	Int currentCount = TheScriptEngine->getObjectCount(player->getPlayerIndex(), key);

	if (sumOfObjs != currentCount) {
		TheScriptEngine->setObjectCount(player->getPlayerIndex(), key, sumOfObjs);
	}

	// --- lost a unit? ---
	return (sumOfObjs < currentCount);
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamSightedRelationType(Parameter* pTeamParm, Int relationType, Parameter* pObjectType)
{

	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) return false;

	ObjectTypesTemp types;
	objectTypesFromParam(pObjectType, types.m_types);

	// and only stuff that is not dead
	PartitionFilterAlive filterAlive;

	// and only nonstealthed items.
	PartitionFilterRejectByObjectStatus filterStealth(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_STEALTHED),
		MAKE_OBJECT_STATUS_MASK2(OBJECT_STATUS_DETECTED, OBJECT_STATUS_DISGUISED));


	for (DLINK_ITERATOR<Object> teamIter = pTeam->iterate_TeamMemberList(); !teamIter.done(); teamIter.advance())
	{
		Object* obj = teamIter.cur();
		
		// and only on-map (or not)
		PartitionFilterSameMapStatus filterMapStatus(obj);

		PartitionFilter* filters[] = { &filterAlive, &filterStealth, &filterMapStatus, nullptr };

		Real visionRange = obj->getShroudClearingRange();
		if (visionRange <= 0.0f) continue;

		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(obj, visionRange, FROM_CENTER_2D, filters);
		MemoryPoolObjectHolder hold(iter);

		for (Object* them = iter->first(); them; them = iter->next())
		{
			if (them == obj)
				continue;

			if (obj->getStatusBits().test(OBJECT_STATUS_STEALTHED) && !obj->getStatusBits().test(OBJECT_STATUS_DETECTED) && !obj->getStatusBits().test(OBJECT_STATUS_DISGUISED))
				continue;

			if (them->getRelationship(obj) == relationType) {
				if (types.m_types->isInSet(them->getTemplate()->getName()))
					return true;
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateSkirmishAnyRelationFaction(Parameter* pPlayerParm, Int relationType, Parameter* pFactionParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer)return false;

	for (int i = 2; i < ThePlayerList->getPlayerCount() - 1; i++)
	{
		if (ThePlayerList->getNthPlayer(i)->getRelationship(pPlayer->getDefaultTeam()) == relationType && ThePlayerList->getNthPlayer(i) != pPlayer)
		{
			if (ThePlayerList->getNthPlayer(i)->getSide() == pFactionParm->getString())
			{
				return true;
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMapSize(Parameter* pComparisonParm, Int pMapSize)
{
	Int actualSize = 0;
	for (Int i = 1; i < MAX_PLAYER_COUNT; i++)
	{
		AsciiString pSpot;
		pSpot.format("Player_%d_Start", i);
		Waypoint* way = TheTerrainLogic->getWaypointByName(pSpot);
		if (!way) break;
		actualSize++;
	}
	switch (pComparisonParm->getInt()) {
		case Parameter::LESS_THAN:			return actualSize < pMapSize;
		case Parameter::LESS_EQUAL:			return actualSize <= pMapSize;
		case Parameter::EQUAL:					return actualSize == pMapSize;
		case Parameter::GREATER_EQUAL:	return actualSize >= pMapSize;
		case Parameter::GREATER:				return actualSize > pMapSize;
		case Parameter::NOT_EQUAL:			return actualSize != pMapSize;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateActivePlayerCount(Parameter* pComparisonParm, Int pPlayerCount)
{
	Int actualCount = 0;
	for (Int i = 2; i < ThePlayerList->getPlayerCount() - 1; i++)
	{
		Player* pPlayers = ThePlayerList->getNthPlayer(i);
		if (pPlayers->isPlayerActive())
		{
			actualCount++;
		}
	}
	switch (pComparisonParm->getInt()) {
		case Parameter::LESS_THAN:			return actualCount < pPlayerCount;
		case Parameter::LESS_EQUAL:			return actualCount <= pPlayerCount;
		case Parameter::EQUAL:					return actualCount == pPlayerCount;
		case Parameter::GREATER_EQUAL:	return actualCount >= pPlayerCount;
		case Parameter::GREATER:				return actualCount > pPlayerCount;
		case Parameter::NOT_EQUAL:			return actualCount != pPlayerCount;
	}
    return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateEmptySpotCount(Parameter* pComparisonParm, Int pEmptySpotCount)
{
	Int activePlayerCount = 0;
	for (Int i = 2; i < ThePlayerList->getPlayerCount() - 1; i++)
	{
		Player* pPlayers = ThePlayerList->getNthPlayer(i);
		if (pPlayers->isPlayerActive())
		{
			activePlayerCount++;
		}
	}
	Int actualMapSize = 0;
	for (Int i = 1; i < MAX_PLAYER_COUNT; i++)
	{
		AsciiString pSpot;
		pSpot.format("Player_%d_Start", i);
		Waypoint* way = TheTerrainLogic->getWaypointByName(pSpot);
		if (!way) break;
		actualMapSize++;
	}
    Int emptySpotCount = actualMapSize - activePlayerCount;
		switch (pComparisonParm->getInt()) {
		case Parameter::LESS_THAN:			return emptySpotCount < pEmptySpotCount;
		case Parameter::LESS_EQUAL:			return emptySpotCount <= pEmptySpotCount;
		case Parameter::EQUAL:					return emptySpotCount == pEmptySpotCount;
		case Parameter::GREATER_EQUAL:	return emptySpotCount >= pEmptySpotCount;
		case Parameter::GREATER:				return emptySpotCount > pEmptySpotCount;
		case Parameter::NOT_EQUAL:			return emptySpotCount != pEmptySpotCount;
		}
		return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerDestroyedEnemyType(Parameter* pPlayerParm, Parameter* pObjectType)
{
	Player* player = playerFromParam(pPlayerParm);
	if (!player) return false;

	const ThingTemplate* tt = TheThingFactory->findTemplate(pObjectType->getString());
	if (!tt) return false;

	
	for (const ThingTemplate* killedTT : player->m_lastFrameKills)
	{
		if (killedTT == tt)
			return true;
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerDestroyedEnemyUnit(Parameter* pPlayerParm)
{
	Player* player = playerFromParam(pPlayerParm);
	if (!player) return false;

	return player->m_lastFrameKills.size() > 0;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerLostUnit(Parameter* pPlayerParm)
{
	Player* player = playerFromParam(pPlayerParm);
	if (!player) return false;

	return player->m_lostUnitThisFrame;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePointControlled(Player* player, const Coord3D& point, Real radius)
{
	for (Player::PlayerTeamList::const_iterator it = player->getPlayerTeams()->begin();
		it != player->getPlayerTeams()->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> teamIter = (*it)->iterate_TeamInstanceList();
			!teamIter.done(); teamIter.advance())
		{
			Team* team = teamIter.cur();
			if (!team)
				continue;

			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList();
				!objIter.done(); objIter.advance())
			{
				Object* obj = objIter.cur();

				if (!obj)
					continue;

				if (obj->isEffectivelyDead())
					continue;

				if (!(obj->isKindOf(KINDOF_STRUCTURE) || obj->isKindOf(KINDOF_VEHICLE) || obj->isKindOf(KINDOF_INFANTRY)))
					continue;

				Real dx = obj->getPosition()->x - point.x;
				Real dy = obj->getPosition()->y - point.y;

				Real distSq = dx * dx + dy * dy;

				if (distSq <= radius * radius)
					return true;
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMapControl(Parameter* pPlayerParm, Parameter* pComparisonParm, Real pValue)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	const Real gridSize = 200.0f;
	const Real controlRadius = 300.0f;

	Int controlled = 0;
	Int total = 0;

  Region3D mapExtent;
  TheTerrainLogic->getExtent(&mapExtent);

	Coord3D mapMin = mapExtent.lo;
	Coord3D mapMax = mapExtent.hi;

	for (Real x = mapMin.x; x < mapMax.x; x += gridSize)
	{
		for (Real y = mapMin.y; y < mapMax.y; y += gridSize)
		{
			total++;

			Coord3D point;
			point.x = x;
			point.y = y;
			point.z = 0;

			if (evaluatePointControlled(pPlayer, point, controlRadius))
				controlled++;
		}
	}

	if (total == 0) return false;

	Real value = pValue / 100.0f;

	switch (pComparisonParm->getInt()) {
	case Parameter::LESS_THAN:			return (Real)controlled / (Real)total < value;
	case Parameter::LESS_EQUAL:			return (Real)controlled / (Real)total <= value;
	case Parameter::EQUAL:					return (Real)controlled / (Real)total == value;
	case Parameter::GREATER_EQUAL:	return (Real)controlled / (Real)total >= value;
	case Parameter::GREATER:				return (Real)controlled / (Real)total > value;
	case Parameter::NOT_EQUAL:			return (Real)controlled / (Real)total != value;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationMapControl(Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Real pValue)
{
	Player* thePlayer = playerFromParam(pPlayerParm);
	if (!thePlayer) return false;

	const Real gridSize = 200.0f;
	const Real controlRadius = 300.0f;

	Int controlled = 0;
	Int total = 0;

	Region3D mapExtent;
	TheTerrainLogic->getExtent(&mapExtent);

	for (Real x = mapExtent.lo.x; x < mapExtent.hi.x; x += gridSize)
	{
		for (Real y = mapExtent.lo.y; y < mapExtent.hi.y; y += gridSize)
		{
			total++;

			Coord3D point;
			point.x = x;
			point.y = y;
			point.z = 0;

			bool pointControlled = false;

			for (int i = 2; i < ThePlayerList->getPlayerCount() - 1; i++)
			{
				Player* pPlayer = ThePlayerList->getNthPlayer(i);
				if (!pPlayer)
					continue;

				if (pPlayer == thePlayer && relationType != 2) //@-TanSo-: if friendly players are being asked for, include me!
					continue;

				if (pPlayer->getRelationship(thePlayer->getDefaultTeam()) != relationType)
					continue;

				if (evaluatePointControlled(pPlayer, point, controlRadius))
				{
					pointControlled = true;
					break;
				}
			}

			if (pointControlled)
				controlled++;
		}
	}
	if (total == 0) return false;

	Real value = pValue / 100.f;

	switch (pComparisonParm->getInt()) {
	case Parameter::LESS_THAN:			return (Real)controlled / (Real)total < value;
	case Parameter::LESS_EQUAL:			return (Real)controlled / (Real)total <= value;
	case Parameter::EQUAL:					return (Real)controlled / (Real)total == value;
	case Parameter::GREATER_EQUAL:	return (Real)controlled / (Real)total >= value;
	case Parameter::GREATER:				return (Real)controlled / (Real)total > value;
	case Parameter::NOT_EQUAL:			return (Real)controlled / (Real)total != value;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateMapControlArea(Parameter* pPlayerParm, Parameter* pComparisonParm, Real pValue, Parameter* pTriggerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	PolygonTrigger* pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pTrig) return false;

	const Real gridSize = 200.0f;
	const Real controlRadius = 300.0f;

	Int controlled = 0;
	Int total = 0;

	Region3D mapExtent;
	TheTerrainLogic->getExtent(&mapExtent);

	Coord3D mapMin = mapExtent.lo;
	Coord3D mapMax = mapExtent.hi;

	for (Real x = mapMin.x; x < mapMax.x; x += gridSize)
	{
		for (Real y = mapMin.y; y < mapMax.y; y += gridSize)
		{
			total++;

			Coord3D point;
			point.x = x;
			point.y = y;
			point.z = 0;

      ICoord3D triggerPoint;
			triggerPoint.x = point.x;
			triggerPoint.y = point.y;
			triggerPoint.z = 0;

			if (pTrig->pointInTrigger(triggerPoint)) {
				if (evaluatePointControlled(pPlayer, point, controlRadius))
					controlled++;
			}
		}
	}

	if (total == 0) return false;

	Real value = pValue / 100.0f;

	switch (pComparisonParm->getInt()) {
	case Parameter::LESS_THAN:			return (Real)controlled / (Real)total < value;
	case Parameter::LESS_EQUAL:			return (Real)controlled / (Real)total <= value;
	case Parameter::EQUAL:					return (Real)controlled / (Real)total == value;
	case Parameter::GREATER_EQUAL:	return (Real)controlled / (Real)total >= value;
	case Parameter::GREATER:				return (Real)controlled / (Real)total > value;
	case Parameter::NOT_EQUAL:			return (Real)controlled / (Real)total != value;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationMapControlArea(Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Real pValue, Parameter* pTriggerParm)
{
	Player* thePlayer = playerFromParam(pPlayerParm);
	if (!thePlayer) return false;

	PolygonTrigger* pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pTrig) return false;

	const Real gridSize = 200.0f;
	const Real controlRadius = 300.0f;

	Int controlled = 0;
	Int total = 0;

	Region3D mapExtent;
	TheTerrainLogic->getExtent(&mapExtent);

	for (Real x = mapExtent.lo.x; x < mapExtent.hi.x; x += gridSize)
	{
		for (Real y = mapExtent.lo.y; y < mapExtent.hi.y; y += gridSize)
		{
			total++;

			Coord3D point;
			point.x = x;
			point.y = y;
			point.z = 0;

			bool pointControlled = false;

			ICoord3D triggerPoint;
			triggerPoint.x = point.x;
			triggerPoint.y = point.y;
			triggerPoint.z = 0;

			for (int i = 2; i < ThePlayerList->getPlayerCount() - 1; i++)
			{
				Player* pPlayer = ThePlayerList->getNthPlayer(i);
				if (!pPlayer)
					continue;

				if (pPlayer == thePlayer && relationType != 2) //@-TanSo-: if friendly players are being asked for, include me!
					continue;

				if (pPlayer->getRelationship(thePlayer->getDefaultTeam()) != relationType)
					continue;

				if (pTrig->pointInTrigger(triggerPoint)) {
					if (evaluatePointControlled(pPlayer, point, controlRadius))
					{
						pointControlled = true;
						break;
					}
				}
			}

			if (pointControlled)
				controlled++;
		}
	}
	if (total == 0) return false;

	Real value = pValue / 100.f;

	switch (pComparisonParm->getInt()) {
	case Parameter::LESS_THAN:			return (Real)controlled / (Real)total < value;
	case Parameter::LESS_EQUAL:			return (Real)controlled / (Real)total <= value;
	case Parameter::EQUAL:					return (Real)controlled / (Real)total == value;
	case Parameter::GREATER_EQUAL:	return (Real)controlled / (Real)total >= value;
	case Parameter::GREATER:				return (Real)controlled / (Real)total > value;
	case Parameter::NOT_EQUAL:			return (Real)controlled / (Real)total != value;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamLostType(Parameter* pTeamParm, Parameter* objectType)
{
	Team* theTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!theTeam)
		return false;

	const ThingTemplate* templ = TheThingFactory->findTemplate(objectType->getString(), FALSE);
	if (templ)
	{
		for (const ThingTemplate* dead : theTeam->m_lastFrameDeaths)
		{
			if (dead == templ)
				return true;
		}
	}
	else
	{
		ObjectTypes* objectTypes = TheScriptEngine->getObjectTypes(objectType->getString());
		if (!objectTypes)
			return false;

		for (size_t i = 0; i < objectTypes->getListSize(); i++)
		{
			const ThingTemplate* type =
				TheThingFactory->findTemplate(objectTypes->getNthInList(i), FALSE);

			if (!type)
				continue;

			for (const ThingTemplate* dead : theTeam->m_lastFrameDeaths)
			{
				if (dead == type)
					return true;
			}
		}
	}

	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamLostUnit(Parameter* pTeamParm)
{
	Team* theTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!theTeam)
		return false;
	return theTeam->m_lostUnitThisFrame;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerSightedRelationType(Parameter* pPlayerParm, Int relationType, Parameter* pObjectType)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	ObjectTypesTemp types;
	objectTypesFromParam(pObjectType, types.m_types);

	// and only stuff that is not dead
	PartitionFilterAlive filterAlive;

	// and only nonstealthed items.
	PartitionFilterRejectByObjectStatus filterStealth(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_STEALTHED),
		MAKE_OBJECT_STATUS_MASK2(OBJECT_STATUS_DETECTED, OBJECT_STATUS_DISGUISED));

	Player::PlayerTeamList::const_iterator it;
	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team* pTeam = iter.cur();
			for (DLINK_ITERATOR<Object> teamIter = pTeam->iterate_TeamMemberList(); !teamIter.done(); teamIter.advance())
			{
				Object* obj = teamIter.cur();

				// and only on-map (or not)
				PartitionFilterSameMapStatus filterMapStatus(obj);

				PartitionFilter* filters[] = { &filterAlive, &filterStealth, &filterMapStatus, nullptr };

				Real visionRange = obj->getShroudClearingRange();
				if (visionRange <= 0.0f) continue;

				SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(obj, visionRange, FROM_CENTER_2D, filters);
				MemoryPoolObjectHolder hold(iter);

				for (Object* them = iter->first(); them; them = iter->next())
				{
					if (them == obj)
						continue;

					if (obj->getStatusBits().test(OBJECT_STATUS_STEALTHED) && !obj->getStatusBits().test(OBJECT_STATUS_DETECTED) && !obj->getStatusBits().test(OBJECT_STATUS_DISGUISED))
						continue;

					if (them->getRelationship(obj) == relationType) {
						if (types.m_types->isInSet(them->getTemplate()->getName()))
							return true;
					}
				}
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerSightedRelationType(Parameter* pPlayerParm, Int playerRelationType, Int relationType, Parameter* pObjectType)
{
  Player* thePlayer = playerFromParam(pPlayerParm);
	if (!thePlayer) return false;

	ObjectTypesTemp types;
	objectTypesFromParam(pObjectType, types.m_types);

	// and only stuff that is not dead
	PartitionFilterAlive filterAlive;

	// and only nonstealthed items.
	PartitionFilterRejectByObjectStatus filterStealth(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_STEALTHED),
		MAKE_OBJECT_STATUS_MASK2(OBJECT_STATUS_DETECTED, OBJECT_STATUS_DISGUISED));

	for(int i = 0; i < ThePlayerList->getPlayerCount(); i++)
	{
    Player* pPlayer = ThePlayerList->getNthPlayer(i);
		if(!pPlayer)
			continue;

		if (pPlayer == thePlayer){
			if (playerRelationType != 2) //@-TanSo-: if friendly players are being asked for, include me!
				continue;
		}
		else{
			if (thePlayer->getRelationship(pPlayer->getDefaultTeam()) != playerRelationType)
				continue;
		}

		Player::PlayerTeamList::const_iterator it;
		for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
		{
			for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
				Team* pTeam = iter.cur();
				for (DLINK_ITERATOR<Object> teamIter = pTeam->iterate_TeamMemberList(); !teamIter.done(); teamIter.advance())
				{
					Object* obj = teamIter.cur();

					// and only on-map (or not)
					PartitionFilterSameMapStatus filterMapStatus(obj);

					PartitionFilter* filters[] = { &filterAlive, &filterStealth, &filterMapStatus, nullptr };

					Real visionRange = obj->getShroudClearingRange();
					if (visionRange <= 0.0f) continue;

					SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(obj, visionRange, FROM_CENTER_2D, filters);
					MemoryPoolObjectHolder hold(iter);

					for (Object* them = iter->first(); them; them = iter->next())
					{
						if (them == obj)
							continue;

						if (obj->getStatusBits().test(OBJECT_STATUS_STEALTHED) && !obj->getStatusBits().test(OBJECT_STATUS_DETECTED) && !obj->getStatusBits().test(OBJECT_STATUS_DISGUISED))
							continue;

						if (them->getRelationship(obj) == relationType) {
							if (types.m_types->isInSet(them->getTemplate()->getName()))
								return true;
						}
					}
				}
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerSightedRelationArea(Parameter* pPlayerParm, Int playerRelationType, Int relationType, Parameter* pTriggerParm)
{
	Player* thePlayer = playerFromParam(pPlayerParm);
	if (!thePlayer) return false;

	PolygonTrigger* pTrigger = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pTrigger) return false;

	// and only stuff that is not dead
	PartitionFilterAlive filterAlive;
	PartitionFilterPolygonTrigger filterArea(pTrigger);
	// and only nonstealthed items.
	PartitionFilterRejectByObjectStatus filterStealth(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_STEALTHED),
		MAKE_OBJECT_STATUS_MASK2(OBJECT_STATUS_DETECTED, OBJECT_STATUS_DISGUISED));

	for (int i = 0; i < ThePlayerList->getPlayerCount(); i++)
	{
		Player* pPlayer = ThePlayerList->getNthPlayer(i);
		if (!pPlayer)
			continue;

		if (pPlayer == thePlayer) {
			if (playerRelationType != 2) //@-TanSo-: if friendly players are being asked for, include me!
				continue;
		}
		else {
			if (thePlayer->getRelationship(pPlayer->getDefaultTeam()) != playerRelationType)
				continue;
		}

		Player::PlayerTeamList::const_iterator it;
		for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
		{
			for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
				Team* pTeam = iter.cur();
				for (DLINK_ITERATOR<Object> teamIter = pTeam->iterate_TeamMemberList(); !teamIter.done(); teamIter.advance())
				{
					Object* obj = teamIter.cur();

					// and only on-map (or not)
					PartitionFilterSameMapStatus filterMapStatus(obj);

					PartitionFilter* filters[] = { &filterAlive, &filterStealth, &filterMapStatus, &filterArea, nullptr };

					Real visionRange = obj->getShroudClearingRange() - 10.0f;
					if (visionRange <= 0.0f) continue;

					SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(obj, visionRange, FROM_CENTER_2D, filters);
					MemoryPoolObjectHolder hold(iter);

					for (Object* them = iter->first(); them; them = iter->next())
					{
						if (them == obj)
							continue;

						if (them->getRelationship(obj) == relationType) {
								return true;
						}
					}
				}
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerSightedRelationTypeArea(Parameter* pPlayerParm, Int playerRelationType, Int relationType, Parameter* pObjectType, Parameter* pTriggerParm)
{
	Player* thePlayer = playerFromParam(pPlayerParm);
	if (!thePlayer) return false;

	ObjectTypesTemp types;
	objectTypesFromParam(pObjectType, types.m_types);

	PolygonTrigger* pTrigger = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pTrigger) return false;

	// and only stuff that is not dead
	PartitionFilterAlive filterAlive;
	PartitionFilterPolygonTrigger filterArea(pTrigger);
	// and only nonstealthed items.
	PartitionFilterRejectByObjectStatus filterStealth(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_STEALTHED),
		MAKE_OBJECT_STATUS_MASK2(OBJECT_STATUS_DETECTED, OBJECT_STATUS_DISGUISED));

	for (int i = 0; i < ThePlayerList->getPlayerCount(); i++)
	{
		Player* pPlayer = ThePlayerList->getNthPlayer(i);
		if (!pPlayer)
			continue;

		if (pPlayer == thePlayer) {
			if (playerRelationType != 2) //@-TanSo-: if friendly players are being asked for, include me!
				continue;
		}
		else {
			if (thePlayer->getRelationship(pPlayer->getDefaultTeam()) != playerRelationType)
				continue;
		}

		Player::PlayerTeamList::const_iterator it;
		for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
		{
			for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
				Team* pTeam = iter.cur();
				for (DLINK_ITERATOR<Object> teamIter = pTeam->iterate_TeamMemberList(); !teamIter.done(); teamIter.advance())
				{
					Object* obj = teamIter.cur();

					// and only on-map (or not)
					PartitionFilterSameMapStatus filterMapStatus(obj);

					PartitionFilter* filters[] = { &filterAlive, &filterStealth, &filterMapStatus, &filterArea, nullptr };

					Real visionRange = obj->getShroudClearingRange();
					if (visionRange <= 0.0f) continue;

					SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(obj, visionRange, FROM_CENTER_2D, filters);
					MemoryPoolObjectHolder hold(iter);

					for (Object* them = iter->first(); them; them = iter->next())
					{
						if (them == obj)
							continue;

						if (obj->getStatusBits().test(OBJECT_STATUS_STEALTHED) && !obj->getStatusBits().test(OBJECT_STATUS_DETECTED) && !obj->getStatusBits().test(OBJECT_STATUS_DISGUISED))
							continue;

						if (them->getRelationship(obj) == relationType) {
							if (types.m_types->isInSet(them->getTemplate()->getName()))
								return true;
						}
					}
				}
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerValueArea(Condition* pCondition,Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Int value, Parameter* pTriggerParm)
{
	Player* sourcePlayer = playerFromParam(pPlayerParm);
	if (!sourcePlayer) return false;

	PolygonTrigger* pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pTrig) return false;

	Bool anyChanges = false;

	if (TheScriptEngine->getFrameObjectCountChanged() != pCondition->getCustomFrame())
	{
		anyChanges = true;
	}

	if (!anyChanges)
	{
		if (pCondition->getCustomData() == -1)
			return false;

		if (pCondition->getCustomData() == 1)
			return true;
	}

	Int totalCost = 0;

	// iterate ALL players
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player* otherPlayer = ThePlayerList->getNthPlayer(i);

		if (!otherPlayer)
			continue;

		if (otherPlayer == sourcePlayer)
				continue;

		if (sourcePlayer->getRelationship(otherPlayer->getDefaultTeam()) != relationType)
				continue;

		// iterate teams
		Player::PlayerTeamList::const_iterator it;

		for (it = otherPlayer->getPlayerTeams()->begin(); it != otherPlayer->getPlayerTeams()->end(); ++it)
		{
			for (DLINK_ITERATOR<Team> teamIter = (*it)->iterate_TeamInstanceList() ;!teamIter.done(); teamIter.advance())
			{
				Team* team = teamIter.cur();

				if (!team)
					continue;

				for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
				{
					Object* pObj = objIter.cur();

					if (!pObj)
						continue;

					if (pObj->isKindOf(KINDOF_INERT))
						continue;

					if (!pObj->isInside(pTrig))
						continue;

					if (pObj->isEffectivelyDead())
						continue;

					const ThingTemplate* tt = pObj->getTemplate();

					if (!tt)
						continue;

					totalCost += tt->friend_getBuildCost();
				}
			}
		}
	}

	Bool comparison = false;

	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:comparison = (totalCost < value);break;
	case Parameter::LESS_EQUAL:comparison = (totalCost <= value);break;
	case Parameter::EQUAL:comparison = (totalCost == value);break;
	case Parameter::GREATER_EQUAL:comparison = (totalCost >= value);break;
	case Parameter::GREATER:comparison = (totalCost > value);break;
	case Parameter::NOT_EQUAL:comparison = (totalCost != value);break;
	}
	pCondition->setCustomData(comparison ? 1 : -1);
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());

	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerOwnsComparisonType(Condition* pCondition, Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Int value, Parameter* objectType)
{
	if (pCondition->getCustomData() != 0)
	{
		// We have a cached value.
		if (TheScriptEngine->getFrameObjectCountChanged() == pCondition->getCustomFrame())
		{
			// object count hasn't changed since we cached.  Use cached value.
			if (pCondition->getCustomData() == 1)
			{
				return true;
			}
			if (pCondition->getCustomData() == -1)
			{
				return false;
			}
		}
	}

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer)
	{
		return false;
	}

	ObjectTypesTemp types;
	objectTypesFromParam(objectType, types.m_types);

		std::vector<Int> counts;
		std::vector<const ThingTemplate*> templates;

		Int numObjs = types.m_types->prepForPlayerCounting(templates, counts);
		Int count = 0;

	for (int i = 0; i < ThePlayerList->getPlayerCount(); i++) {
		if (numObjs > 0)
		{
      Player* cPlayer = ThePlayerList->getNthPlayer(i);
			if (!cPlayer)
				continue;
			if (cPlayer == pPlayer && relationType != ALLIES) // Count me in if ALLIES!
				continue;
      if (cPlayer->getRelationship(pPlayer->getDefaultTeam()) != relationType)
				continue;

			cPlayer->countObjectsByThingTemplate(numObjs, &(*templates.begin()), false, &(*counts.begin()));
			count += rts::sum(counts);
		}

	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:		comparison = (count < value);break;
	case Parameter::LESS_EQUAL:		comparison = (count <= value);break;
	case Parameter::EQUAL:				comparison = (count == value);break;
	case Parameter::GREATER_EQUAL:comparison = (count >= value);break;
	case Parameter::GREATER:			comparison = (count > value);break;
	case Parameter::NOT_EQUAL:		comparison = (count != value);break;
	}
	pCondition->setCustomData(-1); // false.
	if (comparison)
	{
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerAttackedInArea(Parameter* pPlayerParm, Parameter* pArea)
{
  Player* pPlayer = playerFromParam(pPlayerParm);
  if (!pPlayer) return false;
  PolygonTrigger* pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pArea->getString());
  if (!pTrig) return false;

	for (Player::PlayerTeamList::const_iterator it = pPlayer->getPlayerTeams()->begin();
		it != pPlayer->getPlayerTeams()->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> teamIter = (*it)->iterate_TeamInstanceList();
			!teamIter.done(); teamIter.advance())
		{
			Team* team = teamIter.cur();
			if (!team)
				continue;
			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList();
				!objIter.done(); objIter.advance())
			{
				Object* obj = objIter.cur();
				if (!obj)
					continue;

				if (obj->isEffectivelyDead() || obj->isKindOf(KINDOF_INERT) || obj->isKindOf(KINDOF_CRATE))
					continue;

				if (!obj->isInside(pTrig))
					continue;

				//now check whether our object is being attacked
				BodyModuleInterface* theBodyModule = obj->getBodyModule();
				if (!theBodyModule)
					continue;

				const DamageInfo * lastDamageInfo = theBodyModule->getLastDamageInfo();

				ObjectID id = lastDamageInfo->in.m_sourceID;
				Object* pAttacker = TheGameLogic->findObjectByID(id);
				if (!pAttacker) continue;

				Player* attackingPlayer = pAttacker->getControllingPlayer();
				if (!attackingPlayer)
					continue;

				if (attackingPlayer == pPlayer)
					continue;

				Int currentFrame = TheGameLogic->getFrame();
				if (currentFrame != theBodyModule->getLastDamageTimestamp() + 1)
                continue;

				if (pPlayer->getRelationship(attackingPlayer->getDefaultTeam()) == ENEMIES) return true;
			}
		}
  }
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerBuildingBeingCaptured(Parameter* pPlayerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	Player::PlayerTeamList::const_iterator it;
	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team* team = iter.cur();
			if (!team) continue;

			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
			{
				Object* pObj = objIter.cur();
				if (!pObj) continue;

				if (pObj->isKindOf(KINDOF_STRUCTURE) && pObj->isBeingCaptured())
				{
					return true;
				}
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerBuildingBeingCapturedType(Parameter* pPlayerParm, Parameter* objectType)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	const ThingTemplate* templ = TheThingFactory->findTemplate(objectType->getString());
	if (templ)
	{
		Player::PlayerTeamList::const_iterator it;
		for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
		{
			for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
				Team* team = iter.cur();
				if (!team) continue;

				for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
				{
					Object* pObj = objIter.cur();
					if (!pObj) continue;

					if (pObj->getTemplate() != templ) continue;

					if (pObj->isKindOf(KINDOF_STRUCTURE) && pObj->isBeingCaptured())
					{
						return true;
					}
				}
			}
		}
	}
	else {
		ObjectTypes* objectTypes = TheScriptEngine->getObjectTypes(objectType->getString());
		if (objectTypes)
		{
			Player::PlayerTeamList::const_iterator it;
			for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
			{
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) continue;

					for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
					{
						Object* pObj = objIter.cur();
						if (!pObj) continue;

						if (!objectTypes->isInSet(pObj->getTemplate())) continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE) && pObj->isBeingCaptured())
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerBuildingBeingCapturedArea(Parameter* pPlayerParm, Parameter* pTriggerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	PolygonTrigger* pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pTrig) return false;

	Player::PlayerTeamList::const_iterator it;
	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			Team* team = iter.cur();
			if (!team) continue;

			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
			{
				Object* pObj = objIter.cur();
				if (!pObj) continue;

				if (!pObj->isInside(pTrig)) continue;

				if (pObj->isKindOf(KINDOF_STRUCTURE) && pObj->isBeingCaptured())
				{
					return true;
				}
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerBuildingBeingCapturedTypeArea(Parameter* pPlayerParm, Parameter* objectType, Parameter* pTriggerParm)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	PolygonTrigger* pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pTrig) return false;

	const ThingTemplate* templ = TheThingFactory->findTemplate(objectType->getString());
	if (templ)
	{
		Player::PlayerTeamList::const_iterator it;
		for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
		{
			for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
				Team* team = iter.cur();
				if (!team) continue;

				for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
				{
					Object* pObj = objIter.cur();
					if (!pObj) continue;

					if (pObj->getTemplate() != templ) continue;

					if (!pObj->isInside(pTrig)) continue;

					if (pObj->isKindOf(KINDOF_STRUCTURE) && pObj->isBeingCaptured())
					{
						return true;
					}
				}
			}
		}
	}
	else {
		ObjectTypes* objectTypes = TheScriptEngine->getObjectTypes(objectType->getString());
		if (objectTypes)
		{
			Player::PlayerTeamList::const_iterator it;
			for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it)
			{
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) continue;

					for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
					{
						Object* pObj = objIter.cur();
						if (!pObj) continue;

						if (!objectTypes->isInSet(pObj->getTemplate())) continue;

						if (!pObj->isInside(pTrig)) continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE) && pObj->isBeingCaptured())
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerComparisonTypeArea(Condition* pCondition, Parameter* pPlayerParm, Int relationType, Parameter* pComparisonParm, Int value, Parameter* objectType, Parameter* pTriggerParm)
{
	AsciiString triggerName = pTriggerParm->getString();
	PolygonTrigger* pTrig = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (pTrig == nullptr) return false;

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) {
		return false;
	}

	Player::PlayerTeamList::const_iterator it;
	Bool anyChanges = false;
	if (pCondition->getCustomData() == 0) anyChanges = true;


	for (it = pPlayer->getPlayerTeams()->begin(); it != pPlayer->getPlayerTeams()->end(); ++it) {
		if (anyChanges) break;
		for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
			if (anyChanges) break;
			Team* team = iter.cur();
			if (!team) {
				continue;
			}
			if (team->didEnterOrExit()) {
				anyChanges = true;
			}
		}
	}
	if (TheScriptEngine->getFrameObjectCountChanged() > pCondition->getCustomFrame()) {
		anyChanges = true; // Objects were added/deleted since we cached, so count could have changed.  jba.
	}
	if (!anyChanges) {
		if (pCondition->getCustomData() == -1) return false;
		if (pCondition->getCustomData() == 1) return true;

	}

	Int count = 0;

  const ThingTemplate* templ = TheThingFactory->findTemplate(objectType->getString());
	if (templ) {
		for (int i = 0; i < ThePlayerList->getPlayerCount(); i++)
		{
      Player* cPlayer = ThePlayerList->getNthPlayer(i);
			if (ThePlayerList->getNthPlayer(i)->getRelationship(pPlayer->getDefaultTeam()) != relationType)
				continue;
			if (ThePlayerList->getNthPlayer(i) == pPlayer)
				continue;

			for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team)
						continue;

					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj) 
							continue;

						if (pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT))
							continue;

						if (!pObj->isInside(pTrig))
							continue;

						if (pObj->getTemplate() != templ)
							continue;

						count++;
					}
				}
			}
		}
	}
	else {
    ObjectTypes* types = TheScriptEngine->getObjectTypes(objectType->getString());
		if (types)
		{
			for (int i = 0; i < ThePlayerList->getPlayerCount(); i++)
			{
				Player* cPlayer = ThePlayerList->getNthPlayer(i);
				if (ThePlayerList->getNthPlayer(i)->getRelationship(pPlayer->getDefaultTeam()) != relationType)
					continue;
				if (ThePlayerList->getNthPlayer(i) == pPlayer)
					continue;

				for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
					for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
						Team* team = iter.cur();
						if (!team)
							continue;

						for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
							Object* pObj = iter.cur();
							if (!pObj)
								continue;

							if (pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT))
								continue;

							if (!pObj->isInside(pTrig))
								continue;

							if (!types->isInSet(pObj->getTemplate()))
								continue;

							count++;
						}
					}
				}
			}
		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
		case Parameter::LESS_THAN:			comparison = (count < value); break;
		case Parameter::LESS_EQUAL:			comparison = (count <= value); break;
		case Parameter::EQUAL:					comparison = (count == value); break;
		case Parameter::GREATER_EQUAL:	comparison = (count >= value); break;
		case Parameter::GREATER:				comparison = (count > value); break;
		case Parameter::NOT_EQUAL:			comparison = (count != value); break;
	}
	pCondition->setCustomData(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamIdle(Parameter* pTeamParm)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) return false;
	//@-TanSo-: how was something like this not in the game from the get-go? ^^
	return pTeam->isIdle();
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonRatioOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* pOther)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	Player* tPlayer = playerFromParam(pOther);
	if (!tPlayer) return false;

	Player::PlayerTeamList::const_iterator it;
	Bool anyChanges = false;
	if (pCondition->getCustomData() == 0) anyChanges = true;

	if (TheScriptEngine->getFrameObjectCountChanged() > pCondition->getCustomFrame()) {
		anyChanges = true; // Objects were added/deleted since we cached, so count could have changed.  jba.
	}
	if (!anyChanges) {
		if (pCondition->getCustomData() == -1) return false;
		if (pCondition->getCustomData() == 1) return true;

	}

	Int count = 0;
	Int countOther = 0;

	for (int i = 0; i < ThePlayerList->getPlayerCount(); i++)
	{
		Player* cPlayer = ThePlayerList->getNthPlayer(i);
		if (cPlayer == pPlayer)
		{
			for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) {
						continue;
					}
					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj)
							continue;

						if ((pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) && !pObj->isKindOf(KINDOF_CRATE))
							continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE))
							continue;

						count++;
					}
				}
			}
		}
		if (cPlayer == tPlayer)
		{
			for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) {
						continue;
					}
					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj)
							continue;

						if ((pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) && !pObj->isKindOf(KINDOF_CRATE))
							continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE))
							continue;

						countOther++;
					}
				}
			}
		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:			comparison = (count < countOther / ratio); break;
	case Parameter::LESS_EQUAL:			comparison = (count <= countOther / ratio); break;
	case Parameter::EQUAL:					comparison = (count == countOther * ratio); break;
	case Parameter::GREATER_EQUAL:	comparison = (count >= countOther * ratio); break;
	case Parameter::GREATER:				comparison = (count > countOther * ratio); break;
	case Parameter::NOT_EQUAL:			comparison = (count != countOther * ratio); break;
	}
	pCondition->setCustomData(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonRatioTypeOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* pOther, Parameter* otherObjectType)
{
	if (pCondition->getCustomData() != 0)
	{
		// We have a cached value.
		if (TheScriptEngine->getFrameObjectCountChanged() == pCondition->getCustomFrame())
		{
			// object count hasn't changed since we cached.  Use cached value.
			if (pCondition->getCustomData() == 1)
			{
				return true;
			}
			if (pCondition->getCustomData() == -1)
			{
				return false;
			}
		}
	}

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	Player* tPlayer = playerFromParam(pOther);
	if (!tPlayer) return false;

	ObjectTypesTemp types;
	ObjectTypesTemp typesOther;
	objectTypesFromParam(objectType, types.m_types);
	objectTypesFromParam(otherObjectType, typesOther.m_types);

	std::vector<Int> counts;
	std::vector<Int> countsOther;
	std::vector<const ThingTemplate*> templates;
	std::vector<const ThingTemplate*> templatesOther;

	Int numObjs = types.m_types->prepForPlayerCounting(templates, counts);
	Int numObjsOther = typesOther.m_types->prepForPlayerCounting(templatesOther, countsOther);
	Int count = 0;
	Int countOther = 0;


	if (numObjs > 0)
	{
		pPlayer->countObjectsByThingTemplate(numObjs, &(*templates.begin()), false, &(*counts.begin()));
		count += rts::sum(counts);
	}

	if (numObjsOther > 0)
	{
		tPlayer->countObjectsByThingTemplate(numObjsOther, &(*templatesOther.begin()), false, &(*countsOther.begin()));
		countOther += rts::sum(countsOther);
	}


	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:		comparison = ((Real)count * ratio < (Real)countOther); break;
	case Parameter::LESS_EQUAL:		comparison = ((Real)count * ratio <= (Real)countOther); break;
	case Parameter::EQUAL:				comparison = ((Real)count == (Real)countOther * ratio); break;
	case Parameter::GREATER_EQUAL:comparison = ((Real)count >= (Real)countOther * ratio); break;
	case Parameter::GREATER:			comparison = ((Real)count > (Real)countOther * ratio); break;
	case Parameter::NOT_EQUAL:		comparison = ((Real)count != (Real)countOther * ratio); break;
	}
	pCondition->setCustomData(-1); // false.
	if (comparison)
	{
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonRatioAreaOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* pTriggerParm, Parameter* pOther)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	Player* tPlayer = playerFromParam(pOther);
	if (!tPlayer) return false;

	PolygonTrigger* pArea = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pArea) return false;

	Player::PlayerTeamList::const_iterator it;
	Bool anyChanges = false;
	if (pCondition->getCustomData() == 0) anyChanges = true;

	if (TheScriptEngine->getFrameObjectCountChanged() > pCondition->getCustomFrame()) {
		anyChanges = true; // Objects were added/deleted since we cached, so count could have changed.  jba.
	}
	if (!anyChanges) {
		if (pCondition->getCustomData() == -1) return false;
		if (pCondition->getCustomData() == 1) return true;

	}

	Int count = 0;
	Int countOther = 0;

	for (int i = 0; i < ThePlayerList->getPlayerCount(); i++)
	{
		Player* cPlayer = ThePlayerList->getNthPlayer(i);
		if (cPlayer == pPlayer)
		{
			for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) {
						continue;
					}
					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj)
							continue;

						if ((pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) && !pObj->isKindOf(KINDOF_CRATE))
							continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE))
							continue;

						if (!pObj->isInside(pArea))
							continue;

						count++;
					}
				}
			}
		}
		if (cPlayer == tPlayer)
		{
			for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) {
						continue;
					}
					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj)
							continue;

						if ((pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) && !pObj->isKindOf(KINDOF_CRATE))
							continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE))
							continue;

						if (!pObj->isInside(pArea))
							continue;

						countOther++;
					}
				}
			}
		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:			comparison = (count < countOther / ratio); break;
	case Parameter::LESS_EQUAL:			comparison = (count <= countOther / ratio); break;
	case Parameter::EQUAL:					comparison = (count == countOther * ratio); break;
	case Parameter::GREATER_EQUAL:	comparison = (count >= countOther * ratio); break;
	case Parameter::GREATER:				comparison = (count > countOther * ratio); break;
	case Parameter::NOT_EQUAL:			comparison = (count != countOther * ratio); break;
	}
	pCondition->setCustomData(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluatePlayerHasComparisonRatioTypeAreaOther(Condition* pCondition, Parameter* pPlayerParm, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* pTriggerParm, Parameter* pOther, Parameter* otherObjectType)
{
	if (pCondition->getCustomData() != 0)
	{
		// We have a cached value.
		if (TheScriptEngine->getFrameObjectCountChanged() == pCondition->getCustomFrame())
		{
			// object count hasn't changed since we cached.  Use cached value.
			if (pCondition->getCustomData() == 1)
			{
				return true;
			}
			if (pCondition->getCustomData() == -1)
			{
				return false;
			}
		}
	}

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	Player* tPlayer = playerFromParam(pOther);
	if (!tPlayer) return false;

	PolygonTrigger* pArea = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pArea) return false;

	ObjectTypesTemp types;
	ObjectTypesTemp typesOther;
	objectTypesFromParam(objectType, types.m_types);
	objectTypesFromParam(otherObjectType, typesOther.m_types);

	std::vector<Int> counts;
	std::vector<Int> countsOther;
	std::vector<const ThingTemplate*> templates;
	std::vector<const ThingTemplate*> templatesOther;

	Int numObjs = types.m_types->prepForPlayerCounting(templates, counts);
	Int numObjsOther = typesOther.m_types->prepForPlayerCounting(templatesOther, countsOther);
	Int count = 0;
	Int countOther = 0;


	if (numObjs > 0)
	{
		pPlayer->countObjectsByThingTemplateArea(numObjs, &(*templates.begin()), false, &(*counts.begin()), false, pArea);
		count += rts::sum(counts);
	}

	if (numObjsOther > 0)
	{
		tPlayer->countObjectsByThingTemplateArea(numObjsOther, &(*templatesOther.begin()), false, &(*countsOther.begin()), false, pArea);
		countOther += rts::sum(countsOther);
	}


	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:		comparison = ((Real)count * ratio < (Real)countOther); break;
	case Parameter::LESS_EQUAL:		comparison = ((Real)count * ratio <= (Real)countOther); break;
	case Parameter::EQUAL:				comparison = ((Real)count == (Real)countOther * ratio); break;
	case Parameter::GREATER_EQUAL:comparison = ((Real)count >= (Real)countOther * ratio); break;
	case Parameter::GREATER:			comparison = ((Real)count > (Real)countOther * ratio); break;
	case Parameter::NOT_EQUAL:		comparison = ((Real)count != (Real)countOther * ratio); break;
	}
	pCondition->setCustomData(-1); // false.
	if (comparison)
	{
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerHasComparisonRatioOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Int pOtherRelation)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	Player::PlayerTeamList::const_iterator it;
	Bool anyChanges = false;
	if (pCondition->getCustomData() == 0) anyChanges = true;

	if (TheScriptEngine->getFrameObjectCountChanged() > pCondition->getCustomFrame()) {
		anyChanges = true; // Objects were added/deleted since we cached, so count could have changed.  jba.
	}
	if (!anyChanges) {
		if (pCondition->getCustomData() == -1) return false;
		if (pCondition->getCustomData() == 1) return true;

	}

	Int count = 0;
	Int countOther = 0;

	for (int i = 0; i < ThePlayerList->getPlayerCount(); i++)
	{
		Player* cPlayer = ThePlayerList->getNthPlayer(i);
		if (cPlayer->getRelationship(pPlayer->getDefaultTeam()) == pRelation)
		{
			if (cPlayer == pPlayer && pRelation != ALLIES) // Count me in if ALLIES!
				continue;

			for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) {
						continue;
					}
					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj)
							continue;

						if ((pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) && !pObj->isKindOf(KINDOF_CRATE))
							continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE))
							continue;

						count++;
					}
				}
			}

		}
		if (cPlayer->getRelationship(pPlayer->getDefaultTeam()) == pOtherRelation)
		{
			if (cPlayer == pPlayer && pOtherRelation != ALLIES) // Count me in if ALLIES!
				continue;

			for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) {
						continue;
					}
					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj)
							continue;

						if ((pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) && !pObj->isKindOf(KINDOF_CRATE))
							continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE))
							continue;

						countOther++;
					}
				}
			}
		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:			comparison = (count < countOther / ratio); break;
	case Parameter::LESS_EQUAL:			comparison = (count <= countOther / ratio); break;
	case Parameter::EQUAL:					comparison = (count == countOther * ratio); break;
	case Parameter::GREATER_EQUAL:	comparison = (count >= countOther * ratio); break;
	case Parameter::GREATER:				comparison = (count > countOther * ratio); break;
	case Parameter::NOT_EQUAL:			comparison = (count != countOther * ratio); break;
	}
	pCondition->setCustomData(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerHasComparisonRatioTypeOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Int pOtherRelation, Parameter* otherObjectType)
{
	if (pCondition->getCustomData() != 0)
	{
		// We have a cached value.
		if (TheScriptEngine->getFrameObjectCountChanged() == pCondition->getCustomFrame())
		{
			// object count hasn't changed since we cached.  Use cached value.
			if (pCondition->getCustomData() == 1)
			{
				return true;
			}
			if (pCondition->getCustomData() == -1)
			{
				return false;
			}
		}
	}

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	ObjectTypesTemp types;
	ObjectTypesTemp typesOther;
	objectTypesFromParam(objectType, types.m_types);
	objectTypesFromParam(otherObjectType, typesOther.m_types);

	std::vector<Int> counts;
	std::vector<Int> countsOther;
	std::vector<const ThingTemplate*> templates;
	std::vector<const ThingTemplate*> templatesOther;

	Int numObjs = types.m_types->prepForPlayerCounting(templates, counts);
	Int numObjsOther = typesOther.m_types->prepForPlayerCounting(templatesOther, countsOther);
	Int count = 0;
	Int countOther = 0;

	for (int i = 0; i < ThePlayerList->getPlayerCount(); i++) {
		Player* cPlayer = ThePlayerList->getNthPlayer(i);
		if (!cPlayer)
			continue;

		if (cPlayer->getRelationship(pPlayer->getDefaultTeam()) == pRelation)
		{
			if (cPlayer == pPlayer && pRelation != ALLIES) // Count me in if ALLIES!
				continue;

			if (numObjs > 0)
			{
				cPlayer->countObjectsByThingTemplate(numObjs, &(*templates.begin()), false, &(*counts.begin()));
				count += rts::sum(counts);
			}
		}

		if (cPlayer->getRelationship(pPlayer->getDefaultTeam()) == pOtherRelation)
		{
			if (cPlayer == pPlayer && pOtherRelation != ALLIES) // Count me in if ALLIES!
				continue;

			if (numObjsOther > 0)
			{
				cPlayer->countObjectsByThingTemplate(numObjsOther, &(*templatesOther.begin()), false, &(*countsOther.begin()));
				countOther += rts::sum(countsOther);
			}
		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:		comparison = ((Real)count < (Real)countOther / ratio); break;
	case Parameter::LESS_EQUAL:		comparison = ((Real)count <= (Real)countOther / ratio); break;
	case Parameter::EQUAL:				comparison = ((Real)count == (Real)countOther * ratio); break;
	case Parameter::GREATER_EQUAL:comparison = ((Real)count >= (Real)countOther * ratio); break;
	case Parameter::GREATER:			comparison = ((Real)count > (Real)countOther * ratio); break;
	case Parameter::NOT_EQUAL:		comparison = ((Real)count != (Real)countOther * ratio); break;
	}
	pCondition->setCustomData(-1); // false.
	if (comparison)
	{
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerHasComparisonRatioAreaOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Parameter* pTriggerParm, Int pOtherRelation)
{
	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;

	PolygonTrigger* pArea = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pArea) return false;

	Player::PlayerTeamList::const_iterator it;
	Bool anyChanges = false;
	if (pCondition->getCustomData() == 0) anyChanges = true;

	if (TheScriptEngine->getFrameObjectCountChanged() > pCondition->getCustomFrame()) {
		anyChanges = true; // Objects were added/deleted since we cached, so count could have changed.  jba.
	}
	if (!anyChanges) {
		if (pCondition->getCustomData() == -1) return false;
		if (pCondition->getCustomData() == 1) return true;

	}

	Int count = 0;
	Int countOther = 0;

	for (int i = 0; i < ThePlayerList->getPlayerCount(); i++)
	{
		Player* cPlayer = ThePlayerList->getNthPlayer(i);
		if (cPlayer->getRelationship(pPlayer->getDefaultTeam()) == pRelation)
		{
			if (cPlayer == pPlayer && pRelation != ALLIES) // Count me in if ALLIES!
				continue;

			for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) {
						continue;
					}
					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj)
							continue;

						if ((pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) && !pObj->isKindOf(KINDOF_CRATE))
							continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE))
							continue;

						if (!pObj->isInside(pArea))
							continue;

						count++;
					}
				}
			}

		}
		if (cPlayer->getRelationship(pPlayer->getDefaultTeam()) == pOtherRelation)
		{
			if (cPlayer == pPlayer && pOtherRelation != ALLIES) // Count me in if ALLIES!
				continue;

			for (it = cPlayer->getPlayerTeams()->begin(); it != cPlayer->getPlayerTeams()->end(); ++it) {
				for (DLINK_ITERATOR<Team> iter = (*it)->iterate_TeamInstanceList(); !iter.done(); iter.advance()) {
					Team* team = iter.cur();
					if (!team) {
						continue;
					}
					for (DLINK_ITERATOR<Object> iter = team->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
						Object* pObj = iter.cur();
						if (!pObj)
							continue;

						if ((pObj->isEffectivelyDead() || pObj->isKindOf(KINDOF_INERT)) && !pObj->isKindOf(KINDOF_CRATE))
							continue;

						if (pObj->isKindOf(KINDOF_STRUCTURE))
							continue;

						if (!pObj->isInside(pArea))
							continue;

						countOther++;
					}
				}
			}
		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:			comparison = (count < countOther / ratio); break;
	case Parameter::LESS_EQUAL:			comparison = (count <= countOther / ratio); break;
	case Parameter::EQUAL:					comparison = (count == countOther * ratio); break;
	case Parameter::GREATER_EQUAL:	comparison = (count >= countOther * ratio); break;
	case Parameter::GREATER:				comparison = (count > countOther * ratio); break;
	case Parameter::NOT_EQUAL:			comparison = (count != countOther * ratio); break;
	}
	pCondition->setCustomData(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateRelationPlayerHasComparisonRatioTypeAreaOtherRelation(Condition* pCondition, Parameter* pPlayerParm, Int pRelation, Parameter* pComparisonParm, Real ratio, Parameter* objectType, Parameter* pTriggerParm, Int pOtherRelation, Parameter* otherObjectType)
{
	if (pCondition->getCustomData() != 0)
	{
		// We have a cached value.
		if (TheScriptEngine->getFrameObjectCountChanged() == pCondition->getCustomFrame())
		{
			// object count hasn't changed since we cached.  Use cached value.
			if (pCondition->getCustomData() == 1)
			{
				return true;
			}
			if (pCondition->getCustomData() == -1)
			{
				return false;
			}
		}
	}

	Player* pPlayer = playerFromParam(pPlayerParm);
	if (!pPlayer) return false;


	PolygonTrigger* pArea = TheScriptEngine->getQualifiedTriggerAreaByName(pTriggerParm->getString());
	if (!pArea) return false;

	ObjectTypesTemp types;
	ObjectTypesTemp typesOther;
	objectTypesFromParam(objectType, types.m_types);
	objectTypesFromParam(otherObjectType, typesOther.m_types);

	std::vector<Int> counts;
	std::vector<Int> countsOther;
	std::vector<const ThingTemplate*> templates;
	std::vector<const ThingTemplate*> templatesOther;

	Int numObjs = types.m_types->prepForPlayerCounting(templates, counts);
	Int numObjsOther = typesOther.m_types->prepForPlayerCounting(templatesOther, countsOther);
	Int count = 0;
	Int countOther = 0;

	for (int i = 0; i < ThePlayerList->getPlayerCount(); i++) {
		Player* cPlayer = ThePlayerList->getNthPlayer(i);
		if (!cPlayer)
			continue;

		if (cPlayer->getRelationship(pPlayer->getDefaultTeam()) == pRelation)
		{
			if (cPlayer == pPlayer && pRelation != ALLIES) // Count me in if ALLIES!
				continue;

			if (numObjs > 0)
			{
				cPlayer->countObjectsByThingTemplateArea(numObjs, &(*templates.begin()), false, &(*counts.begin()), false, pArea);
				count += rts::sum(counts);
			}
		}

		if (cPlayer->getRelationship(pPlayer->getDefaultTeam()) == pOtherRelation)
		{
			if (cPlayer == pPlayer && pOtherRelation != ALLIES) // Count me in if ALLIES!
				continue;

			if (numObjsOther > 0)
			{
				cPlayer->countObjectsByThingTemplateArea(numObjsOther, &(*templatesOther.begin()), false, &(*countsOther.begin()), false, pArea);
				countOther += rts::sum(countsOther);
			}

		}
	}

	Bool comparison = false;
	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:		comparison = ((Real)count < (Real)countOther / ratio); break;
	case Parameter::LESS_EQUAL:		comparison = ((Real)count <= (Real)countOther / ratio); break;
	case Parameter::EQUAL:				comparison = ((Real)count == (Real)countOther * ratio); break;
	case Parameter::GREATER_EQUAL:comparison = ((Real)count >= (Real)countOther * ratio); break;
	case Parameter::GREATER:			comparison = ((Real)count > (Real)countOther * ratio); break;
	case Parameter::NOT_EQUAL:		comparison = ((Real)count != (Real)countOther * ratio); break;
	}
	pCondition->setCustomData(-1); // false.
	if (comparison)
	{
		pCondition->setCustomData(1); // true.
	}
	pCondition->setCustomFrame(TheScriptEngine->getFrameObjectCountChanged());
	return comparison;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamContainsType(Parameter* pTeamParm, Parameter* objectType)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) return false;

	const ThingTemplate* templ = TheThingFactory->findTemplate(objectType->getString());
	if (templ)
	{
		for (DLINK_ITERATOR<Object> iter = pTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
			Object* pObj = iter.cur();
			if (!pObj)
				continue;

			if (pObj->getTemplate() == templ)
				return true;
		}
	}
	else {
		ObjectTypes* types = TheScriptEngine->getObjectTypes(objectType->getString());
		if (types)
		{
			for (DLINK_ITERATOR<Object> iter = pTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object* pObj = iter.cur();
				if (!pObj)
					continue;

				if (types->isInSet(pObj->getTemplate()))
					return true;
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamContainsComparisonType(Parameter* pTeamParm, Parameter* pComparisonParm, Int value, Parameter* objectType)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) return false;

	Int count = 0;

	const ThingTemplate* templ = TheThingFactory->findTemplate(objectType->getString());
	if (templ)
	{
		for (DLINK_ITERATOR<Object> iter = pTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
			Object* pObj = iter.cur();
			if (!pObj)
				continue;

			if (pObj->getTemplate() == templ)
				count++;
		}
	}
	else {
		ObjectTypes* types = TheScriptEngine->getObjectTypes(objectType->getString());
		if (types)
		{
			for (DLINK_ITERATOR<Object> iter = pTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
				Object* pObj = iter.cur();
				if (!pObj)
					continue;

				if (types->isInSet(pObj->getTemplate()))
					count++;
			}
		}
	}

	switch (pComparisonParm->getInt())
	{
	case Parameter::LESS_THAN:			return value < count; break;
	case Parameter::LESS_EQUAL:			return value <= count; break;
	case Parameter::EQUAL:					return value = count; break;
	case Parameter::GREATER_EQUAL:	return value >= count; break;
	case Parameter::GREATER:				return value > count; break;
	case Parameter::NOT_EQUAL:			return value != count; break;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamSingleBelowHealth(Parameter* pTeamParm, Real value)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) return false;
	

	for (DLINK_ITERATOR<Object> objIter = pTeam->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
	{
		Object* pObj = objIter.cur();
		if (!pObj) continue;

		BodyModuleInterface* pBody = pObj->getBodyModule();
		if (!pBody) continue;

		if (pBody->getMaxHealth() * (value / 100.0f) > pBody->getHealth()) return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamBelowHealth(Parameter* pTeamParm, Real value)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) return false;

	return pTeam->m_maxHealth * (value / 100.0f) > pTeam->getCurrentHealth();
}

//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateTeamIdleFrames(Parameter* pTeamParm, Int value)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) return false;

	return pTeam->m_idleFrames >= value;
}


Bool ScriptConditions::evaluateTeamSeen(Parameter* pTeamParm)
{
	Team* pTeam = TheScriptEngine->getTeamNamed(pTeamParm->getString());
	if (!pTeam) return false;

	for (DLINK_ITERATOR<Object> iter = pTeam->iterate_TeamMemberList(); !iter.done(); iter.advance()) {
		Object* pObj = iter.cur();
		if (!pObj) {
			continue;
		}

		if (pObj->m_seenByEnemy)
			return true;
	}
	return false;
}
//-------------------------------------------------------------------------------------------------
//---------------------------- @CLP_AI SCRIPT CONDITION ADDITIONS END -----------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/** Evaluate a condition */
//-------------------------------------------------------------------------------------------------
Bool ScriptConditions::evaluateCondition( Condition *pCondition )
{
	switch (pCondition->getConditionType()) {
		default:
			DEBUG_CRASH(("Unknown ScriptCondition type %d", pCondition->getConditionType()));
			return false;
		case Condition::PLAYER_ALL_DESTROYED:
			return evaluateAllDestroyed(pCondition->getParameter(0));
		case Condition::PLAYER_ALL_BUILDFACILITIES_DESTROYED:
			return evaluateAllBuildFacilitiesDestroyed(pCondition->getParameter(0));
		case Condition::TEAM_INSIDE_AREA_PARTIALLY:
			return evaluateTeamInsideAreaPartially(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::NAMED_INSIDE_AREA:
			return evaluateNamedInsideArea(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_DESTROYED:
			return evaluateIsDestroyed(pCondition->getParameter(0));
		case Condition::NAMED_DESTROYED:
			return evaluateNamedUnitDestroyed(pCondition->getParameter(0));
		case Condition::NAMED_DYING:
			return evaluateNamedUnitDying(pCondition->getParameter(0));
		case Condition::NAMED_TOTALLY_DEAD:
			return evaluateNamedUnitTotallyDead(pCondition->getParameter(0));
		case Condition::NAMED_NOT_DESTROYED:
			return evaluateNamedUnitExists(pCondition->getParameter(0));
		case Condition::TEAM_HAS_UNITS:
			return evaluateHasUnits(pCondition->getParameter(0));
		case Condition::CAMERA_MOVEMENT_FINISHED:
			return TheTacticalView->isCameraMovementFinished();
		case Condition::TEAM_STATE_IS:
			return evaluateTeamStateIs(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_STATE_IS_NOT:
			return evaluateTeamStateIsNot(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::NAMED_OUTSIDE_AREA:
			return evaluateNamedOutsideArea(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_INSIDE_AREA_ENTIRELY:
			return evaluateTeamInsideAreaEntirely(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::TEAM_OUTSIDE_AREA_ENTIRELY:
			return evaluateTeamOutsideAreaEntirely(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::NAMED_ATTACKED_BY_OBJECTTYPE:
			return evaluateNamedAttackedByType(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_ATTACKED_BY_OBJECTTYPE:
			return evaluateTeamAttackedByType(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::NAMED_ATTACKED_BY_PLAYER:
			return evaluateNamedAttackedByPlayer(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_ATTACKED_BY_PLAYER:
			return evaluateTeamAttackedByPlayer(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::BUILT_BY_PLAYER:
			return evaluateBuiltByPlayer(pCondition, pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::NAMED_CREATED:
			return evaluateNamedCreated(pCondition->getParameter(0));
		case Condition::TEAM_CREATED:
			return evaluateTeamCreated(pCondition->getParameter(0));
		case Condition::PLAYER_HAS_CREDITS:
			return evaluatePlayerHasCredits(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::BRIDGE_REPAIRED:
			return evaluateBridgeRepaired(pCondition->getParameter(0));
		case Condition::BRIDGE_BROKEN:
			return evaluateBridgeBroken(pCondition->getParameter(0));
		case Condition::NAMED_DISCOVERED:
			return evaluateNamedDiscovered(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_DISCOVERED:
			return evaluateTeamDiscovered(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::BUILDING_ENTERED_BY_PLAYER:
			return evaluateBuildingEntered(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::ENEMY_SIGHTED:
		{
			Int numParameters = pCondition->getNumParameters();
			DEBUG_ASSERTCRASH(numParameters == 3, ("'Condition: [Unit] Unit has sighted a(n) friendly/neutral/enemy unit belonging to a side.' has too few parameters. Please fix in WB. (jkmcd)"));

			if (numParameters < 3) {
				return false;
			}

			return evaluateEnemySighted(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		}
		case Condition::TYPE_SIGHTED:
			return evaluateTypeSighted(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::NAMED_BUILDING_IS_EMPTY:
			return evaluateIsBuildingEmpty(pCondition->getParameter(0));
		case Condition::MISSION_ATTEMPTS:
			return evaluateMissionAttempts(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::NAMED_OWNED_BY_PLAYER:
			return evaluateNamedOwnedByPlayer(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_OWNED_BY_PLAYER:
			return evaluateTeamOwnedByPlayer(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::PLAYER_HAS_N_OR_FEWER_BUILDINGS:
			return evaluatePlayerHasNOrFewerBuildings(pCondition->getParameter(1), pCondition->getParameter(0));
		case Condition::PLAYER_HAS_N_OR_FEWER_FACTION_BUILDINGS:
			return evaluatePlayerHasNOrFewerFactionBuildings(pCondition->getParameter(1), pCondition->getParameter(0));
		case Condition::PLAYER_HAS_POWER:
			return evaluatePlayerHasPower(pCondition->getParameter(0));
		case Condition::PLAYER_HAS_NO_POWER:
			return !evaluatePlayerHasPower(pCondition->getParameter(0));
		case Condition::NAMED_REACHED_WAYPOINTS_END:
			return evaluateNamedReachedWaypointsEnd(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_REACHED_WAYPOINTS_END:
			return evaluateTeamReachedWaypointsEnd(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::NAMED_SELECTED:
			return evaluateNamedSelected(pCondition, pCondition->getParameter(0));
		case Condition::NAMED_ENTERED_AREA:
			return evaluateNamedEnteredArea(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::NAMED_EXITED_AREA:
			return evaluateNamedExitedArea(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_ENTERED_AREA_ENTIRELY:
			return evaluateTeamEnteredAreaEntirely(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::TEAM_ENTERED_AREA_PARTIALLY:
			return evaluateTeamEnteredAreaPartially(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::TEAM_EXITED_AREA_ENTIRELY:
			return evaluateTeamExitedAreaEntirely(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::TEAM_EXITED_AREA_PARTIALLY:
			return evaluateTeamExitedAreaPartially(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::MULTIPLAYER_ALLIED_VICTORY:
			return evaluateMultiplayerAlliedVictory();
		case Condition::MULTIPLAYER_ALLIED_DEFEAT:
			return evaluateMultiplayerAlliedDefeat();
		case Condition::MULTIPLAYER_PLAYER_DEFEAT:
			return evaluateMultiplayerPlayerDefeat();
		case Condition::HAS_FINISHED_VIDEO:
			return evaluateVideoHasCompleted(pCondition->getParameter(0));
		case Condition::HAS_FINISHED_SPEECH:
			return evaluateSpeechHasCompleted(pCondition->getParameter(0));
		case Condition::HAS_FINISHED_AUDIO:
			return evaluateAudioHasCompleted(pCondition->getParameter(0));
		case Condition::UNIT_HEALTH:
			return evaluateUnitHealth(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::PLAYER_TRIGGERED_SPECIAL_POWER:
			return evaluatePlayerSpecialPowerFromUnitTriggered(pCondition->getParameter(0), pCondition->getParameter(1), nullptr);
		case Condition::PLAYER_TRIGGERED_SPECIAL_POWER_FROM_NAMED:
			return evaluatePlayerSpecialPowerFromUnitTriggered(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::PLAYER_MIDWAY_SPECIAL_POWER:
			return evaluatePlayerSpecialPowerFromUnitMidway(pCondition->getParameter(0), pCondition->getParameter(1), nullptr);
		case Condition::PLAYER_MIDWAY_SPECIAL_POWER_FROM_NAMED:
			return evaluatePlayerSpecialPowerFromUnitMidway(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::PLAYER_COMPLETED_SPECIAL_POWER:
			return evaluatePlayerSpecialPowerFromUnitComplete(pCondition->getParameter(0), pCondition->getParameter(1), nullptr);
		case Condition::PLAYER_COMPLETED_SPECIAL_POWER_FROM_NAMED:
			return evaluatePlayerSpecialPowerFromUnitComplete(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::PLAYER_ACQUIRED_SCIENCE:
			return evaluateScienceAcquired(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::PLAYER_CAN_PURCHASE_SCIENCE:
			return evaluateCanPurchaseScience(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::PLAYER_HAS_SCIENCEPURCHASEPOINTS:
			return evaluateSciencePurchasePoints(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::NAMED_HAS_FREE_CONTAINER_SLOTS:
			return evaluateNamedHasFreeContainerSlots(pCondition->getParameter(0));
		case Condition::DEFUNCT_PLAYER_SELECTED_GENERAL:
		case Condition::DEFUNCT_PLAYER_SELECTED_GENERAL_FROM_NAMED:
			DEBUG_CRASH(("PLAYER_SELECTED_GENERAL script conditions are no longer in use"));
			return false;
		case Condition::PLAYER_BUILT_UPGRADE:
			return evaluateUpgradeFromUnitComplete(pCondition->getParameter(0), pCondition->getParameter(1), nullptr);
		case Condition::PLAYER_BUILT_UPGRADE_FROM_NAMED:
			return evaluateUpgradeFromUnitComplete(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::PLAYER_HAS_OBJECT_COMPARISON:
			return evaluatePlayerUnitCondition(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2), pCondition->getParameter(3));
		case Condition::PLAYER_DESTROYED_N_BUILDINGS_PLAYER:
			return evaluatePlayerDestroyedNOrMoreBuildings(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::PLAYER_HAS_COMPARISON_UNIT_TYPE_IN_TRIGGER_AREA:
		{
			Int numParameters = pCondition->getNumParameters();
			DEBUG_ASSERTCRASH(numParameters == 5, ("'Condition: [Player] has (comparison) unit type in an area' has too few parameters. Please fix in WB. (jkmcd)"));

			if (numParameters < 5) {
				return false;
			}

			return evaluatePlayerHasUnitTypeInArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2), pCondition->getParameter(3), pCondition->getParameter(4));
		}
		case Condition::PLAYER_HAS_COMPARISON_UNIT_KIND_IN_TRIGGER_AREA:
		{
			Int numParameters = pCondition->getNumParameters();
			DEBUG_ASSERTCRASH(numParameters == 5, ("'Condition: [Player] has (comparison) kind of unit or structure in an area' has too few parameters. Please fix in WB. (jkmcd)"));

			if (numParameters < 5) {
				return false;
			}

			return evaluatePlayerHasUnitKindInArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2), pCondition->getParameter(3), pCondition->getParameter(4));
		}
		case Condition::UNIT_EMPTIED:
			return evaluateUnitHasEmptied(pCondition->getParameter(0));

		case Condition::PLAYER_POWER_COMPARE_PERCENT:
			return evaluatePlayerHasComparisonPercentPower(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));

		case Condition::PLAYER_EXCESS_POWER_COMPARE_VALUE:
			return evaluatePlayerHasComparisonValueExcessPower(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));

		case Condition::SKIRMISH_SPECIAL_POWER_READY:
			return evaluateSkirmishSpecialPowerIsReady(pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::UNIT_HAS_OBJECT_STATUS:
			return evaluateUnitHasObjectStatus(pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::TEAM_ALL_HAS_OBJECT_STATUS:
			return evaluateTeamHasObjectStatus(pCondition->getParameter(0), pCondition->getParameter(1), true);

		case Condition::TEAM_SOME_HAVE_OBJECT_STATUS:
			return evaluateTeamHasObjectStatus(pCondition->getParameter(0), pCondition->getParameter(1), false);

		case Condition::SKIRMISH_VALUE_IN_AREA:
			return evaluateSkirmishValueInArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2), pCondition->getParameter(3));

		case Condition::SKIRMISH_PLAYER_FACTION:
			return evaluateSkirmishPlayerIsFaction(pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::SKIRMISH_SUPPLIES_VALUE_WITHIN_DISTANCE:
			return evaluateSkirmishSuppliesWithinDistancePerimeter(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2), pCondition->getParameter(3));

		case Condition::SKIRMISH_TECH_BUILDING_WITHIN_DISTANCE:
			return evaluateSkirmishPlayerTechBuildingWithinDistancePerimeter(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));

		case Condition::SKIRMISH_COMMAND_BUTTON_READY_ALL:
			return evaluateSkirmishCommandButtonIsReady(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2), true);

		case Condition::SKIRMISH_COMMAND_BUTTON_READY_PARTIAL:
			return evaluateSkirmishCommandButtonIsReady(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2), false);

		case Condition::SKIRMISH_UNOWNED_FACTION_UNIT_EXISTS:
			return evaluateSkirmishUnownedFactionUnitComparison(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));

		case Condition::SKIRMISH_PLAYER_HAS_PREREQUISITE_TO_BUILD:
			return evaluateSkirmishPlayerHasPrereqsToBuild(pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::SKIRMISH_PLAYER_HAS_COMPARISON_GARRISONED:
			return evaluateSkirmishPlayerHasComparisonGarrisoned(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));

		case Condition::SKIRMISH_PLAYER_HAS_COMPARISON_CAPTURED_UNITS:
			return evaluateSkirmishPlayerHasComparisonCapturedUnits(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));

		case Condition::SKIRMISH_NAMED_AREA_EXIST:
			return evaluateSkirmishNamedAreaExists(pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::SKIRMISH_PLAYER_HAS_UNITS_IN_AREA:
			return evaluateSkirmishPlayerHasUnitsInArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::SKIRMISH_PLAYER_HAS_BEEN_ATTACKED_BY_PLAYER:
			return evaluateSkirmishPlayerHasBeenAttackedByPlayer(pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::SKIRMISH_PLAYER_IS_OUTSIDE_AREA:
			return evaluateSkirmishPlayerIsOutsideArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::SKIRMISH_PLAYER_HAS_DISCOVERED_PLAYER:
			return evaluateSkirmishPlayerHasDiscoveredPlayer(pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::MUSIC_TRACK_HAS_COMPLETED:
			return evaluateMusicHasCompleted(pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::SUPPLY_SOURCE_SAFE:
			return evaluateSkirmishSupplySourceSafe(pCondition, pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::SUPPLY_SOURCE_ATTACKED:
			return evaluateSkirmishSupplySourceAttacked(pCondition->getParameter(0));

		case Condition::START_POSITION_IS:
			return evaluateSkirmishStartPosition(pCondition->getParameter(0), pCondition->getParameter(1));

		case Condition::PLAYER_LOST_OBJECT_TYPE:
			return evaluatePlayerLostObjectType(pCondition->getParameter(0), pCondition->getParameter(1));



		case Condition::RELATION_IS:
			return evaluatePlayerRelation(pCondition->getParameter(0)->getString(), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2)->getString());
		case Condition::SPOT_EMPTY:
      return evaluateEmptySpot(pCondition->getParameter(0));
    case Condition::SPOT_NEIGHBOURING:
      return evaluateNeighbouringSpot(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::NEIGHBOURING_SPOTS_EMPTY:
			return evaluateNeighbouringSpotsEmpty(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getInt());
		case Condition::NEIGHBOURING_SPOTS_RELATION:
      return evaluateNeighbouringSpotsRelation(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3)->getInt());
		case Condition::STARTING_CASH_COMPARE:
      return evaluateStartingCash(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
		case Condition::CLOSEST_ENEMY_DISTANCE:
			return evaluateClosestRelationUnitToMySpawn(pCondition->getParameter(0)->getInt(), pCondition->getParameter(1), pCondition->getParameter(2), pCondition->getParameter(3)->getReal());
		case Condition::PLAYER_HUNTED:
			return evaluateHunted(pCondition->getParameter(0));
		case Condition::PLAYER_LOST_TYPE_AREA:
			return evaluatePlayerLostTypeInArea(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
		case Condition::TEAM_SIGHTED_RELATION_TYPE:
			return evaluateTeamSightedRelationType(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2));
		case Condition::PLAYER_RELATION_FACTION:
			return evaluateSkirmishAnyRelationFaction(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2));
		case Condition::MAP_COMPARE_SIZE:
			return evaluateMapSize(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
		case Condition::MAP_ACTIVE_PLAYERS:
			return evaluateActivePlayerCount(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
		case Condition::MAP_EMPTY_SPOTS:
			return evaluateEmptySpotCount(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
		case Condition::PLAYER_DESTROYED_ENEMY_TYPE:
			return evaluatePlayerDestroyedEnemyType(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::PLAYER_DESTROYED_ENEMY_UNIT:
			return evaluatePlayerDestroyedEnemyUnit(pCondition->getParameter(0));
		case Condition::PLAYER_LOST_UNIT:
			return evaluatePlayerLostUnit(pCondition->getParameter(0));
		case Condition::PLAYER_MAPCONTROL:
			return evaluateMapControl(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal());
		case Condition::PLAYER_RELATION_MAPCONTROL:
			return evaluateRelationMapControl(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal());
		case Condition::PLAYER_MAPCONTROL_AREA:
			return evaluateMapControlArea(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3));
		case Condition::PLAYER_RELATION_MAPCONTROL_AREA:
			return evaluateRelationMapControlArea(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4));
		case Condition::TEAM_LOST_TYPE:
      return evaluateTeamLostType(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_LOST_UNIT:
			return evaluateTeamLostUnit(pCondition->getParameter(0));
		case Condition::PLAYER_SIGHTED_RELATION_TYPE:
      return evaluatePlayerSightedRelationType(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2));
    case Condition::RELATION_PLAYER_SIGHTED_RELATION_TYPE:
      return evaluateRelationPlayerSightedRelationType(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3));
		case Condition::RELATION_PLAYER_SIGHTED_RELATION_AREA:
			return evaluateRelationPlayerSightedRelationArea(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3));
		case Condition::RELATION_PLAYER_SIGHTED_RELATION_TYPE_AREA:
			return evaluateRelationPlayerSightedRelationTypeArea(pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3), pCondition->getParameter(4));
		case Condition::RELATION_PLAYER_VALUE_AREA:
      return evaluateRelationPlayerValueArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getInt(),pCondition->getParameter(4));
    case Condition::RELATION_PLAYER_OWNS_COMPARISON_TYPE:
      return evaluateRelationPlayerOwnsComparisonType(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getInt(), pCondition->getParameter(4));
		case Condition::PLAYER_ATTACKED_AREA:
      return evaluatePlayerAttackedInArea(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::PLAYER_BUILDING_BEING_CAPTURED:
      return evaluatePlayerBuildingBeingCaptured(pCondition->getParameter(0));
		case Condition::PLAYER_BUILDING_BEING_CAPTURED_TYPE:
			return evaluatePlayerBuildingBeingCapturedType(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::PLAYER_BUILDING_BEING_CAPTURED_AREA:
			return evaluatePlayerBuildingBeingCapturedArea(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::PLAYER_BUILDING_BEING_CAPTURED_TYPE_AREA:
			return evaluatePlayerBuildingBeingCapturedTypeArea(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2));
    case Condition::RELATION_PLAYER_COMPARISON_TYPE_AREA:
			return evaluateRelationPlayerComparisonTypeArea(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getInt(), pCondition->getParameter(4), pCondition->getParameter(5));
		case Condition::TEAM_IDLE:
			return evaluateTeamIdle(pCondition->getParameter(0));
		case Condition::PLAYER_COMPARISON_RATIO_OTHER:
			return evaluatePlayerHasComparisonRatioOther(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3));
		case Condition::PLAYER_COMPARISON_RATIO_TYPE_OTHER:
			return evaluatePlayerHasComparisonRatioTypeOther(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3), pCondition->getParameter(4), pCondition->getParameter(5));
		case Condition::PLAYER_COMPARISON_RATIO_AREA_OTHER:
			return evaluatePlayerHasComparisonRatioAreaOther(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3), pCondition->getParameter(4));
		case Condition::PLAYER_COMPARISON_RATIO_TYPE_AREA_OTHER:
			return evaluatePlayerHasComparisonRatioTypeAreaOther(pCondition, pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getReal(), pCondition->getParameter(3), pCondition->getParameter(4), pCondition->getParameter(5), pCondition->getParameter(6));
		case Condition::RELATION_PLAYER_COMPARISON_RATIO_OTHER_RELATION:
			return evaluateRelationPlayerHasComparisonRatioOtherRelation(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4)->getInt());
		case Condition::RELATION_PLAYER_COMPARISON_RATIO_TYPE_OTHER_RELATION:
			return evaluateRelationPlayerHasComparisonRatioTypeOtherRelation(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4), pCondition->getParameter(5)->getInt(), pCondition->getParameter(6));
		case Condition::RELATION_PLAYER_COMPARISON_RATIO_AREA_OTHER_RELATION:
			return evaluateRelationPlayerHasComparisonRatioAreaOtherRelation(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4), pCondition->getParameter(5)->getInt());
		case Condition::RELATION_PLAYER_COMPARISON_RATIO_TYPE_AREA_OTHER_RELATION:
			return evaluateRelationPlayerHasComparisonRatioTypeAreaOtherRelation(pCondition, pCondition->getParameter(0), pCondition->getParameter(1)->getInt(), pCondition->getParameter(2), pCondition->getParameter(3)->getReal(), pCondition->getParameter(4), pCondition->getParameter(5), pCondition->getParameter(6)->getInt(), pCondition->getParameter(7));
		case Condition::TEAM_CONTAINS_TYPE:
			return evaluateTeamContainsType(pCondition->getParameter(0), pCondition->getParameter(1));
		case Condition::TEAM_CONTAINS_COMPARISON_TYPE:
			return evaluateTeamContainsComparisonType(pCondition->getParameter(0), pCondition->getParameter(1), pCondition->getParameter(2)->getInt(), pCondition->getParameter(3));
		case Condition::TEAM_SINGLE_BELOW_HEALTH:
			return evaluateTeamSingleBelowHealth(pCondition->getParameter(0), pCondition->getParameter(1)->getReal());
		case Condition::TEAM_BELOW_HEALTH:
			return evaluateTeamBelowHealth(pCondition->getParameter(0), pCondition->getParameter(1)->getReal());
		case Condition::TEAM_IDLE_FRAMES:
			return evaluateTeamIdleFrames(pCondition->getParameter(0), pCondition->getParameter(1)->getInt());
		case Condition::TEAM_SEEN:
			return evaluateTeamSeen(pCondition->getParameter(0));
	}
}
