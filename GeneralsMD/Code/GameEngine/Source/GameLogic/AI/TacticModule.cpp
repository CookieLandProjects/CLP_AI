////////////////////////////////////////////////////////////////////////////////
//																																						//
//  CLP_AI Improved AI Project																								//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// TacticModule.cpp
// Tactical evaluation system for the AIPlayer
// Author: -TanSo-, November 2026

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "Common/GameMemory.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/AIPlayer.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TacticModule.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"

//------------------------------------------------------------------------------------------------
TacticModule::TacticModule(Player* p) :
	m_player(p),
	m_table(nullptr),

	m_rawStrength(0.0f),
	m_relativeStrength(0.0f),
	m_rawTeamStrength(0.0f),
	m_relativeTeamStrength(0.0f),

	m_rawEconomy(0.0f),
	m_relativeEconomy(0.0f),
	m_rawTeamEconomy(0.0f),
	m_relativeTeamEconomy(0.0f),

	m_mapControl(0.0f),
	m_teamMapControl(0.0f)
{
	Region3D mapExtent;
	TheTerrainLogic->getExtent(&mapExtent);

	Coord3D mapMin = mapExtent.lo;
	Coord3D mapMax = mapExtent.hi;

	for (Real y = mapMin.y; y <= mapMax.y; y += MAP_CONTROL_GRID_INTERVAL)
	{
		for (Real x = mapMin.x; x <= mapMax.x; x += MAP_CONTROL_GRID_INTERVAL)
		{
			MapControlNode node;
			node.m_pos.x = x;
			node.m_pos.y = y;
			node.m_pos.z = 0.0f;
			node.m_isControlled = FALSE;
			node.m_threatLevel = 0.0f;
			node.m_defenseLevel = 0.0f;
			m_mapControlGrid.push_back(node);
		}
	}
	m_table = new UnitTypeTable();
}

//------------------------------------------------------------------------------------------------
// update - once in a while, no need to do this every frame.
//------------------------------------------------------------------------------------------------
void TacticModule::update()
{
	// Update 4x/10sec @60fps, 2x/10sec @30fps
	if (TheGameLogic->getFrame() % 150 != 0)
		return;

	updateStrengthAndEconomy();

	updateUnitTypeTable();

	updateMapControl();

	processMapControl();

	updateResourceInfo();

	processResourceInfo();

	updateFlankInfo(); // ALMOST DONE

	processFlankInfo();

}

//------------------------------------------------------------------------------------------------
// xfer - Snapshot method
//------------------------------------------------------------------------------------------------
void TacticModule::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);

	if (version >= 1){
		xfer->xferReal(&m_rawStrength);
		xfer->xferReal(&m_relativeStrength);
		xfer->xferReal(&m_rawTeamStrength);
		xfer->xferReal(&m_relativeTeamStrength);
		xfer->xferReal(&m_rawEconomy);
		xfer->xferReal(&m_relativeEconomy);
		xfer->xferReal(&m_rawTeamEconomy);
		xfer->xferReal(&m_relativeTeamEconomy);
		xfer->xferReal(&m_mapControl);
		xfer->xferReal(&m_teamMapControl);

		for (Int i = 0; i < m_mapControlGrid.size(); i++) {
			xfer->xferCoord3D(&m_mapControlGrid[i].m_pos);
			xfer->xferBool(&m_mapControlGrid[i].m_isControlled);
			xfer->xferReal(&m_mapControlGrid[i].m_threatLevel);
			xfer->xferReal(&m_mapControlGrid[i].m_defenseLevel);
		}

		for (Int i = 0; i < m_ownedResources.size(); i++) {
			xfer->xferObjectID(&m_ownedResources[i].m_id);
			xfer->xferReal(&m_ownedResources[i].m_threatLevel);
			xfer->xferReal(&m_ownedResources[i].m_defenseLevel);
			xfer->xferReal(&m_ownedResources[i].m_strategicValue);
		}

		for (Int i = 0; i < m_currentFlanks.size(); i++) {
			for (Int j = 0; j < m_currentFlanks[i].m_path.size(); j++)
			{
				xfer->xferCoord3D(&m_currentFlanks[i].m_path[j].m_pos);
				xfer->xferBool(&m_currentFlanks[i].m_path[j].m_isControlled);
				xfer->xferReal(&m_currentFlanks[i].m_path[j].m_threatLevel);
				xfer->xferReal(&m_currentFlanks[i].m_path[j].m_defenseLevel);
			}
			xfer->xferReal(&m_currentFlanks[i].m_threatLevel);
			xfer->xferReal(&m_currentFlanks[i].m_defenseLevel);
		}
	}
}

//------------------------------------------------------------------------------------------------
// crc - Snapshot method
//------------------------------------------------------------------------------------------------
void TacticModule::crc(Xfer* xfer)
{

}

//------------------------------------------------------------------------------------------------
// loadPostProcess - Snapshot method
//------------------------------------------------------------------------------------------------
void TacticModule::loadPostProcess()
{

}

//------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------
Bool TacticModule::shouldAttack()
{
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------
Bool TacticModule::shouldHarass()
{
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------
Bool TacticModule::shouldExpand()
{
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------
Bool TacticModule::shouldBuildByResource(Object* resource)
{
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// shouldContestSupplies
//------------------------------------------------------------------------------------------------
Bool TacticModule::shouldContestResource(Object* resource)
{
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// shouldProtectSupplies - is it worth protecting the given resource?
//------------------------------------------------------------------------------------------------
Bool TacticModule::shouldProtectResource(Object* resource)
{
	if(!resource)
		return FALSE;

	ResourceInfo info;
	if (getTacticInfoEntry(info, resource))
	{
		// It is barely worth investing because any theroretical returns are miniscule
		if (info.m_strategicValue < 0.25f)
			return FALSE;

		return info.m_defenseLevel <= info.m_threatLevel * info.m_strategicValue;
	}
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------
Bool TacticModule::shouldTurtle()
{
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------
Bool TacticModule::shouldRegroup()
{
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------
Bool TacticModule::shouldRetreat()
{
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// getTacticInfoEntry - Put the result into info if found and return true, else return false.
//------------------------------------------------------------------------------------------------
Bool TacticModule::getTacticInfoEntry(ResourceInfo& info, Object* resource)
{
	if (!resource)
		return FALSE;

	ObjectID resourceID = resource->getID();
	for (Int i = 0; i < m_ownedResources.size(); i++)
	{
		if (resourceID == m_ownedResources[i].m_id) {
			info = m_ownedResources[i];
			return TRUE;
		}
	}
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// updateStrength - Do all player and team strength calculations
//------------------------------------------------------------------------------------------------
void TacticModule::updateStrengthAndEconomy()
{
	Real strength = 0.0f;
	Real teamStrength = 0.0f;
	Real enemyStrength = 0.0f;

	Real economy = 0.0f;
	Real teamEconomy = 0.0f;
	Real enemyEconomy = 0.0f;

	Int enemyCount = 0;

	for (Int i = 0; i < ThePlayerList->getPlayerCount(); i++)
	{
		Player* other = ThePlayerList->getNthPlayer(i);
		if (!other) continue;

		if (other->getRelationship(m_player->getDefaultTeam()) == ENEMIES)
			enemyCount++;

		for (Player::PlayerTeamList::const_iterator it = other->getPlayerTeams()->begin(); it != other->getPlayerTeams()->end(); ++it)
		{
			for (DLINK_ITERATOR<Team> teamIter = (*it)->iterate_TeamInstanceList(); !teamIter.done(); teamIter.advance())
			{
				Team* team = teamIter.cur();
				if (!team)
					continue;

				for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
				{
					Object* obj = objIter.cur();
					if (!obj)
						continue;

					Real buildCost = obj->getTemplate()->friend_getBuildCost();

					if (obj->isKindOf(KINDOF_CAN_ATTACK) && !obj->isKindOf(KINDOF_STRUCTURE))
					{
						Real maxHP = obj->getBodyModule()->getMaxHealth();
						Real curHP = obj->getBodyModule()->getHealth();
						Real strengthBoost = buildCost * (curHP / maxHP);

						if (other == m_player) {
							strength += strengthBoost;
							teamStrength += strengthBoost;
						}
						if (other->getRelationship(m_player->getDefaultTeam()) == ALLIES) {
							teamStrength += strengthBoost;
						}
						if (other->getRelationship(m_player->getDefaultTeam()) == ENEMIES) {
							enemyStrength += strengthBoost;
						}
					}
					else if (obj->isKindOf(KINDOF_MONEY_HACKER) ||
						obj->isKindOf(KINDOF_FS_SUPPLY_DROPZONE) ||
						obj->isKindOf(KINDOF_FS_BLACK_MARKET))
					{
						if (other == m_player) {
							economy += buildCost;
							teamEconomy += buildCost;
						}
						if (other->getRelationship(m_player->getDefaultTeam()) == ALLIES) {
							teamEconomy += buildCost;
						}
						if (other->getRelationship(m_player->getDefaultTeam()) == ENEMIES) {
							enemyEconomy += buildCost;
						}
					}
				}
			}
		}
	}
	enemyStrength /= enemyCount;
	enemyEconomy /= enemyCount;

	m_rawStrength = strength;
	m_rawTeamStrength = teamStrength;
	m_relativeStrength = strength / (strength + enemyStrength);
	m_relativeTeamStrength = teamStrength / (teamStrength + enemyStrength);

	m_rawEconomy = economy;
	m_rawTeamEconomy = teamEconomy;
	m_relativeEconomy = economy / (economy + enemyEconomy);
	m_relativeTeamEconomy = teamEconomy / (teamEconomy + enemyEconomy);
}

//------------------------------------------------------------------------------------------------
// updateUnitTypeTable - Calculate the map control we have
//------------------------------------------------------------------------------------------------
void TacticModule::updateUnitTypeTable()
{
	const ThingTemplate* humveeTemplate = TheThingFactory->findTemplate("AmericaVehicleHumvee");
	// The player could have upgraded the TOW-Missile. In that case, add the Humvee as an anti-air vehicle.
	// Note: only check AmericaVehicleHumvee, because we will just add all versions of the humvee upon success anyways.
	if (!m_table->hasAnyType(humveeTemplate, UT_ANTI_AIR)) {
		if (m_player->hasUpgradeComplete(TheUpgradeCenter->findUpgrade("Upgrade_AmericaTOWMissile")))
		{
			m_table->add(humveeTemplate, UT_ANTI_AIR);
			m_table->add(TheThingFactory->findTemplate("AirF_AmericaVehicleHumvee"), UT_ANTI_AIR);
			m_table->add(TheThingFactory->findTemplate("Lazr_AmericaVehicleHumvee"), UT_ANTI_AIR);
			m_table->add(TheThingFactory->findTemplate("SupW_AmericaVehicleHumvee"), UT_ANTI_AIR);
		}
	}

	const ThingTemplate* nukeMiGTemplate = TheThingFactory->findTemplate("Nuke_ChinaJetMIG");
	if (!m_table->hasAnyType(nukeMiGTemplate, UT_ANTI_TANK)) {
		if (m_player->hasUpgradeComplete(TheUpgradeCenter->findUpgrade("Upgrade_ChinaTacticalNukeMig")))
		{
			m_table->add(nukeMiGTemplate, UT_ANTI_TANK);
		}
	}

	// TODO: Think about also including stuff like:
	// nuke shell upgrade -> add UT_ANTI_INFANTRY on battlemasters etc once upgraded
}

//------------------------------------------------------------------------------------------------
// updateResourceInfo - Check which resource to throw out, keep, or add.
//------------------------------------------------------------------------------------------------
void TacticModule::updateResourceInfo()
{
	AIPlayer* ai = m_player->getAi();
	if (!ai) return;

	const ThingTemplate* oilTemplate = TheThingFactory->findTemplate("TechOilDerrick");
	const ThingTemplate* refTemplate = TheThingFactory->findTemplate("TechOilRefinery");

	// Do we not have any reservation for this dock? Does this oil or refinery still belong to us?
	for (Int i = 0; i < m_ownedResources.size(); i++)
	{
		Object* obj = TheGameLogic->findObjectByID(m_ownedResources[i].m_id);
		if (!obj || (obj->getControllingPlayer() != m_player && !ai->hasDockReservation(m_ownedResources[i].m_id)))
		{
			m_ownedResources.erase(m_ownedResources.begin() + i);
			i--;
			continue;
		}
	}
	// Do we not yet have a reservation for this dock?
	std::vector<SupplyDockReservation> reservations = ai->getSupplyDockReservations();
	for (Int i = 0; i < reservations.size(); i++)
	{
		Bool isIncluded = FALSE;
		for (Int j = 0; j < m_ownedResources.size(); j++)
		{
			if (reservations[i].dockID == m_ownedResources[j].m_id)
			{
				isIncluded = TRUE;
				break;
			}
		}
		// Add the missing dock
		if (!isIncluded)
		{
			ResourceInfo info;
			info.m_id = reservations[i].dockID;
			info.m_threatLevel = 0.0f;
			info.m_defenseLevel = 0.0f;
			info.m_strategicValue = 0.0f;
			m_ownedResources.push_back(info);
		}
	}

	// See if we are missing an oil or refinery
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		const ThingTemplate* curTemplate = obj->getTemplate();
		if (curTemplate != oilTemplate && curTemplate != refTemplate)
			continue;

		if (obj->getControllingPlayer() != m_player)
			continue;

		// This is an oil or a refinery
		Bool isIncluded = FALSE;
		for (Int i = 0; i < m_ownedResources.size(); i++)
		{
			if (obj->getID() == m_ownedResources[i].m_id)
			{
				isIncluded = TRUE;
				break;
			}
		}
		// Add the missing dock
		if (!isIncluded)
		{
			ResourceInfo info;
			info.m_id = obj->getID();
			info.m_threatLevel = 0.0f;
			info.m_defenseLevel = 0.0f;
			info.m_strategicValue = 0.0f;
			m_ownedResources.push_back(info);
		}
	}
}

//------------------------------------------------------------------------------------------------
// processResourceInfo - Calculate the info for each resource.
//------------------------------------------------------------------------------------------------
void TacticModule::processResourceInfo()
{
	const ThingTemplate* oilTemplate = TheThingFactory->findTemplate("TechOilDerrick");

	for (Int i = 0; i < m_ownedResources.size(); i++)
	{
		Object* me = TheGameLogic->findObjectByID(m_ownedResources[i].m_id);
		// I don't exist anymore. Remove me
		if (!me) {
			m_ownedResources.erase(m_ownedResources.begin() + i);
			i--;
			continue;
		}

		/*
		**
		** m_threatLevel & m_defenseLevel calculation
		**
		*/
		PartitionFilterSameMapStatus f1(me);
		PartitionFilterAlive f2;
		PartitionFilterPlayerAffiliation f3enemy(m_player, ALLOW_ENEMIES, TRUE);
		PartitionFilterPlayerAffiliation f3ally(m_player, ALLOW_ALLIES, TRUE);
		PartitionFilterAcceptByKindOf f4(MAKE_KINDOF_MASK(KINDOF_CAN_ATTACK), KINDOFMASK_NONE);
		PartitionFilter* filtersEnemy[] = { &f1, &f2, &f3enemy, &f4, nullptr };
		PartitionFilter* filtersAlly[] = { &f1, &f2, &f3ally, &f4, nullptr };

		// Anything greater than MAX_DECTECTION_DISTANCE is not close enough to be considered a threat
		SimpleObjectIterator* iterEnemy = ThePartitionManager->iterateObjectsInRange(me, MAX_DECTECTION_DISTANCE, FROM_CENTER_2D, filtersEnemy);
		MemoryPoolObjectHolder holdEnemy(iterEnemy);
		// Allied units further than MAX_DECTECTION_DISTANCE may not get to defend in time
		SimpleObjectIterator* iterAlly = ThePartitionManager->iterateObjectsInRange(me, MAX_DECTECTION_DISTANCE, FROM_CENTER_2D, filtersAlly);
		MemoryPoolObjectHolder holdAlly(iterAlly);

		// m_threatLevel is defined as the joint buildcost of the given four buckets.
		// Allies that specifically counter these units subract their own buildcost from the total.
		// Left are the accumulated buildcosts of the "non-countered" units.
		// These get added together and then put into the range 0 -> 1 with BUILDCOST_NORMALIZATION
		Real bucketAir = 0.0f;
		Real bucketTank = 0.0f;
		Real bucketVehicle = 0.0f;
		Real bucketInfantry = 0.0f;
		Coord3D resourcePos = *me->getPosition();

		for (Object* obj = iterEnemy->first(); obj; obj = iterEnemy->next())
		{
			Coord3D objPos = *obj->getPosition();
			Real distanceModifier = getDistanceModifier(resourcePos, objPos, MAX_DECTECTION_DISTANCE, 0.0f);
			addToThreatLevelBuckets(obj, bucketAir, bucketTank, bucketVehicle, bucketInfantry, distanceModifier);
		}

		Real curDefenseLevel = 0.0f;
		for (Object* obj = iterAlly->first(); obj; obj = iterAlly->next())
		{
			Coord3D objPos = *obj->getPosition();
			Real distanceModifier = getDistanceModifier(resourcePos, objPos, MAX_DECTECTION_DISTANCE, 0.0f);
			subractFromThreatLevelBuckets(obj, bucketAir, bucketTank, bucketVehicle, bucketInfantry, distanceModifier);
			// onto the defense level
			distanceModifier = getDistanceModifier(resourcePos, objPos, MAX_DEFENSE_DISTANCE, 100.0f);
			addToDefenseLevel(obj, curDefenseLevel, distanceModifier);
		}

		Real curThreatLevel = bucketAir + bucketTank + bucketVehicle + bucketInfantry;

		m_ownedResources[i].m_threatLevel = curThreatLevel / (curThreatLevel + BUILDCOST_NORMALIZATION);
		m_ownedResources[i].m_defenseLevel = curDefenseLevel / (curDefenseLevel + BUILDCOST_NORMALIZATION);

		/*
		**
		** m_strategicValue calculation
		**
		*/
		const ThingTemplate* resourceTemplate = me->getTemplate();
		static const NameKeyType key = TheNameKeyGenerator->nameToKey("SupplyWarehouseDockUpdate");
		SupplyWarehouseDockUpdate* warehouseModule = (SupplyWarehouseDockUpdate*)me->findUpdateModule(key);
		// We are a SupplyDock or another similar building like SupplyWarehouse or ToxinRepository
		if (warehouseModule)
		{
			Real supplyAmount = warehouseModule->getBoxesStored() * SUPPLY_BOX_VALUE;

			// We are, more specifically, a SupplyPileSmall
			if (resourceTemplate == TheThingFactory->findTemplate("SupplyPileSmall"))
			{
				// See if we can find other piles around
				Real supplyClusterRadius = 90.0f;
				Real supplyClusterRadiusSquared = supplyClusterRadius * supplyClusterRadius;

				for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
				{
					if (obj == me)
						continue;

					if (obj->getTemplate() != resourceTemplate)
						continue;

					const Coord3D* objPos = obj->getPosition();
					Real dx = objPos->x - resourcePos.x;
					Real dy = objPos->y - resourcePos.y;
					Real distanceSquared = dx * dx + dy * dy;

					if (distanceSquared <= supplyClusterRadiusSquared) {
						SupplyWarehouseDockUpdate* pileModule = (SupplyWarehouseDockUpdate*)obj->findUpdateModule(key);
						if (pileModule)
							supplyAmount += pileModule->getBoxesStored() * SUPPLY_BOX_VALUE;
					}
				}
			}
			m_ownedResources[i].m_strategicValue = supplyAmount / SUPPLY_NORMALIZATION;

			if (m_ownedResources[i].m_strategicValue > 1.0f)
				m_ownedResources[i].m_strategicValue = 1.0f;
		}
		// We are infact NOT a supply source, but a tech building.
		else
		{
			Real estimatedWorth = 1.0f; // If we are a TechOilRefinery, our worth doesn't degrade, so leave me at 1.0f at the end!

			// We are a TechOilDerrick. See how much we are needed by looking at the player's secondary eco.
			if (resourceTemplate == oilTemplate)
			{
				Real ecoAmount = 0.0f;
				for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
				{
					if (obj == me)
						continue;

					if (obj->getControllingPlayer() != m_player)
						continue;

					// 4x Hacker roughly equals 1x BlackMarket or 1x SupplyDropZone
					if (obj->isKindOf(KINDOF_MONEY_HACKER)) {
						ecoAmount = ecoAmount + 12.5f;
						continue;
					}
					if (obj->isKindOf(KINDOF_FS_SUPPLY_DROPZONE)) {
						ecoAmount = ecoAmount + 50.0f;
						continue;
					}
					if (obj->isKindOf(KINDOF_FS_BLACK_MARKET)) {
						ecoAmount = ecoAmount + 50.0f;
						continue;
					}
				}
				// At 20x BMs, 20x SDZs or 80x Hackers it will turn estimatedWorth to 0.5f.
				estimatedWorth = OIL_DERRICK_VALUE / (OIL_DERRICK_VALUE + ecoAmount);
			}
			m_ownedResources[i].m_strategicValue = estimatedWorth;
		}
	}
}

//------------------------------------------------------------------------------------------------
// updateFlankInfo - figure out how flanks have shifted
//------------------------------------------------------------------------------------------------
/*
** The order of nodes in m_mapControlGrid looks like this:
**	16	17	18	19	20
**	11	12	13	14	15
**	06	07	08	09	10
**	01	02	03	04	05
**
** Any node that is not controlled gets ignored:
**	  	  	11
**	  	08	09	10
**	04	05	06	07
**	01	02	03
**
** And then only controlled nodes with an uncontrolled horizontal or vertical neighbour get picked up:
**	  	  	06
**	  	04	  	05
**	02	  	  	03
**	  	  	01
*/
void TacticModule::updateFlankInfo()
{
	m_currentFlanks.clear();

	Region3D mapExtent;
	TheTerrainLogic->getExtent(&mapExtent);

	Coord3D mapMin = mapExtent.lo;
	Coord3D mapMax = mapExtent.hi;

	std::vector<Int> borderNodes;
	Int gridWidth = (mapMax.x / MAP_CONTROL_GRID_INTERVAL) + 1;
	Int gridHeight = (mapMax.y / MAP_CONTROL_GRID_INTERVAL) + 1;

	// Connect flanks that have at most 2 uncontrolled points between each other in any direction.
	connectFlankGridComponents(gridWidth, gridHeight);

	// Throw out anything that is not a border node.
	for (Int i = 0; i < m_mapControlGrid.size(); i++)
	{
		if (!m_mapControlGrid[i].m_isControlled)
			continue;

		Int column = i % gridWidth;
		Int row = i / gridWidth;

		MapControlNode node = m_mapControlGrid[i];
		Bool isBorder = FALSE;

		// left neighbour
		if (column > 0 && !m_mapControlGrid[i - 1].m_isControlled)
			isBorder = TRUE;
		// right neighbour
		if (column < gridWidth - 1 && !m_mapControlGrid[i + 1].m_isControlled)
			isBorder = TRUE;
		// bottom neighbour
		if (row > 0 && !m_mapControlGrid[i - gridWidth].m_isControlled)
			isBorder = TRUE;
		// top neighbour
		if (row < gridHeight - 1 && !m_mapControlGrid[i + gridWidth].m_isControlled)
			isBorder = TRUE;

		if (isBorder) {
			borderNodes.push_back(i);
		}
	}

	// Get all paths sorted out.
	std::vector<std::vector<Coord3D>> paths;
	traceFlankOutlines(gridWidth, gridHeight, borderNodes, paths);

	for (Int i = 0; i < paths.size(); i++)
	{
		if (paths[i].empty())
			continue;

		FlankInfo flank;
		flank.m_threatLevel = 0.0f;
		flank.m_defenseLevel = 0.0f;

		for (Int j = 0; j < paths[i].size(); j++)
		{
			MapControlNode node;
			node.m_pos = paths[i][j];
			node.m_isControlled = TRUE;
			node.m_threatLevel = 0.0f;
			node.m_defenseLevel = 0.0f;
			flank.m_path.push_back(node);
		}
		m_currentFlanks.push_back(flank);
	}

	for (Int i = 0; i < m_debugUnits.size(); i++)
	{
		m_debugUnits[i]->kill();
	}

	m_debugUnits.clear();

	if (m_player->getPlayerDisplayName() != L"Inhumane Army")
		return;

	for (Int i = 0; i < m_currentFlanks.size(); i++)
	{
		for (Int j = 0; j < m_currentFlanks[i].m_path.size(); j++)
		{
			Object* obj = TheThingFactory->newObject(TheThingFactory->findTemplate("ChinaInfantryRedguard"), m_player->getDefaultTeam());
			if (obj)
			{
				Coord3D tmp;
				tmp.x = m_currentFlanks[i].m_path[j].m_pos.x;
				tmp.y = m_currentFlanks[i].m_path[j].m_pos.y;
				tmp.z = TheTerrainLogic->getGroundHeight(tmp.x, tmp.y);
				obj->setOrientation(45.0f);
				obj->setPosition(&tmp);
				m_debugUnits.push_back(obj);
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
// processFlankInfo - recalculate all of our flanks.
//------------------------------------------------------------------------------------------------
void TacticModule::processFlankInfo()
{
	for (Int i = 0; i < m_currentFlanks.size(); i++)
	{
		Real curThreatLevel = 0.0f;
		Real curDefenseLevel = 0.0f;
		Int pathSize = m_currentFlanks[i].m_path.size();

		for (Int j = 0; j < pathSize; j++)
		{
			curThreatLevel += m_currentFlanks[i].m_path[j].m_threatLevel;
			curDefenseLevel += m_currentFlanks[i].m_path[j].m_defenseLevel;
		}
		curThreatLevel /= pathSize;
		curDefenseLevel /= pathSize;

		m_currentFlanks[i].m_threatLevel = curThreatLevel;
		m_currentFlanks[i].m_defenseLevel = curDefenseLevel;
	}
}

//------------------------------------------------------------------------------------------------
// updateMapControl - See how our map control has changed since the last time.
//------------------------------------------------------------------------------------------------
void TacticModule::updateMapControl()
{
	Real controlled = 0;
	Real teamControlled = 0;
	Real total = 0;

	Region3D mapExtent;
	TheTerrainLogic->getExtent(&mapExtent);

	Coord3D mapMin = mapExtent.lo;
	Coord3D mapMax = mapExtent.hi;

	for (Real y = mapMin.y; y < mapMax.y; y += MAP_CONTROL_GRID_INTERVAL)
	{
		for (Real x = mapMin.x; x < mapMax.x; x += MAP_CONTROL_GRID_INTERVAL)
		{
			Coord3D point;
			point.x = x;
			point.y = y;
			point.z = 0.0f;

			// Reset map control to FALSE;
			m_mapControlGrid[total].m_isControlled = FALSE;

			// This is us. Update m_mapControl and the entry in m_mapControlGrid here.
			if (isPointControlled(m_player, point)) {
				m_mapControlGrid[total].m_isControlled = TRUE;

				controlled++;
				teamControlled++;
				total++;
				continue;
			}

			for (Int i = 0; i < ThePlayerList->getPlayerCount() - 1; i++)
			{
				Player* other = ThePlayerList->getNthPlayer(i);
				if (!other)
					continue;

				if (other->getRelationship(m_player->getDefaultTeam()) != ALLIES)
					continue;

				if (m_player == other)
					continue;

				// This is a team mate. Update m_teamMapControl here.
				if (isPointControlled(other, point)) {
					teamControlled++;
					break;
				}
			}
			total++;
		}
	}
	m_mapControl = controlled / total;
	m_teamMapControl = teamControlled / total;
}

//------------------------------------------------------------------------------------------------
// processMapControl - Calculate the map control we have.
//------------------------------------------------------------------------------------------------
void TacticModule::processMapControl()
{
	for (Int i = 0; i < m_mapControlGrid.size(); i++)
	{
		MapControlNode node = m_mapControlGrid[i];
		if (!node.m_isControlled) continue;

		PartitionFilterAlive f1;
		PartitionFilterPlayerAffiliation f2enemy(m_player, ALLOW_ENEMIES, TRUE);
		PartitionFilterPlayerAffiliation f2ally(m_player, ALLOW_ALLIES, TRUE);
		PartitionFilterAcceptByKindOf f3(MAKE_KINDOF_MASK(KINDOF_CAN_ATTACK), KINDOFMASK_NONE);
		PartitionFilter* filtersEnemy[] = { &f1, &f2enemy, &f3, nullptr };
		PartitionFilter* filtersAlly[] = { &f1,&f2ally, &f3, nullptr };

		// Anything greater than MAX_DECTECTION_DISTANCE is not close enough to be considered a threat
		SimpleObjectIterator* iterEnemy = ThePartitionManager->iterateObjectsInRange(&node.m_pos, MAX_DECTECTION_DISTANCE, FROM_CENTER_2D, filtersEnemy);
		MemoryPoolObjectHolder holdEnemy(iterEnemy);
		// Allied units further than MAX_DECTECTION_DISTANCE may not get to defend in time
		SimpleObjectIterator* iterAlly = ThePartitionManager->iterateObjectsInRange(&node.m_pos, MAX_DECTECTION_DISTANCE, FROM_CENTER_2D, filtersAlly);
		MemoryPoolObjectHolder holdAlly(iterAlly);

		// m_threatLevel is defined as the joint buildcost of the given four buckets.
		// Allies that specifically counter these units subract their own buildcost from the total.
		// Left are the accumulated buildcosts of the "non-countered" units.
		// These get added together and then put into the range 0 -> 1 with BUILDCOST_NORMALIZATION
		Real bucketAir = 0.0f;
		Real bucketTank = 0.0f;
		Real bucketVehicle = 0.0f;
		Real bucketInfantry = 0.0f;

		for (Object* obj = iterEnemy->first(); obj; obj = iterEnemy->next())
		{
			Coord3D objPos = *obj->getPosition();
			Real distanceModifier = getDistanceModifier(node.m_pos, objPos, MAX_DECTECTION_DISTANCE, 0.0f);
			addToThreatLevelBuckets(obj, bucketAir, bucketTank, bucketVehicle, bucketInfantry, distanceModifier);
		}

		Real curDefenseLevel = 0.0f;
		for (Object* obj = iterAlly->first(); obj; obj = iterAlly->next())
		{
			Coord3D objPos = *obj->getPosition();
			Real distanceModifier = getDistanceModifier(node.m_pos, objPos, MAX_DECTECTION_DISTANCE, 0.0f);
			subractFromThreatLevelBuckets(obj, bucketAir, bucketTank, bucketVehicle, bucketInfantry, distanceModifier);
			// onto the defense level
			distanceModifier = getDistanceModifier(node.m_pos, objPos, MAX_DEFENSE_DISTANCE, 100.0f);
			addToDefenseLevel(obj, curDefenseLevel, distanceModifier);
		}

		Real curThreatLevel = bucketAir + bucketTank + bucketVehicle + bucketInfantry;

		m_mapControlGrid[i].m_threatLevel = curThreatLevel / (curThreatLevel + BUILDCOST_NORMALIZATION);
		m_mapControlGrid[i].m_defenseLevel = curDefenseLevel / (curDefenseLevel + BUILDCOST_NORMALIZATION);
	}
}

//------------------------------------------------------------------------------------------------
// isPointControlled - See wheter a specific point is well within the player's reach
//------------------------------------------------------------------------------------------------
Bool TacticModule::isPointControlled(Player* p, Coord3D point)
{
	if (!p) return FALSE;

	for (Player::PlayerTeamList::const_iterator it = p->getPlayerTeams()->begin(); it != p->getPlayerTeams()->end(); ++it)
	{
		for (DLINK_ITERATOR<Team> teamIter = (*it)->iterate_TeamInstanceList(); !teamIter.done(); teamIter.advance())
		{
			Team* team = teamIter.cur();
			if (!team)
				continue;

			for (DLINK_ITERATOR<Object> objIter = team->iterate_TeamMemberList(); !objIter.done(); objIter.advance())
			{
				Object* obj = objIter.cur();

				if (!obj)
					continue;

				if (obj->isEffectivelyDead())
					continue;

				// We consider structures as influencial on persistent map control.
				if (!obj->isKindOf(KINDOF_STRUCTURE))
					continue;

				Real dx = obj->getPosition()->x - point.x;
				Real dy = obj->getPosition()->y - point.y;

				Real distSq = dx * dx + dy * dy;

				if (distSq <= MAP_CONTROL_CAPTURE_RADIUS * MAP_CONTROL_CAPTURE_RADIUS)
					return TRUE;
			}
		}
	}
	return FALSE;
}

//------------------------------------------------------------------------------------------------
// connectFlankGridComponents - Hook up closely located flanks.
//------------------------------------------------------------------------------------------------
void TacticModule::connectFlankGridComponents(Int gridWidth, Int gridHeight)
{
	//Grid too smoll
	if (m_mapControlGrid.empty() || gridWidth <= 0 || gridHeight <= 0)
		return;

	const Int dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
	const Int dy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

	const Int gridSize = m_mapControlGrid.size();
	const Int maxConnectionDistance = NODE_GAP + 1;

	// Find all currently connected components.
	while (TRUE)
	{
		// translate
		std::vector<Bool> visited(gridSize, FALSE);
		std::vector<std::vector<Int>> components;

		for (Int i = 0; i < gridSize; i++)
		{
			if (!m_mapControlGrid[i].m_isControlled || visited[i])
				continue;

			std::vector<Int> component;
			std::vector<Int> pending;

			pending.push_back(i);
			visited[i] = TRUE;

			for (Int j = 0; j < pending.size(); j++)
			{
				Int current = pending[j];

				Int x = current % gridWidth;
				Int y = current / gridWidth;

				component.push_back(current);

				for (Int direction = 0; direction < 8; direction++)
				{
					Int nx = x + dx[direction];
					Int ny = y + dy[direction];

					if (nx < 0 || nx >= gridWidth ||
							ny < 0 || ny >= gridHeight)
						continue;

					Int neighbourIndex = ny * gridWidth + nx;

					if (visited[neighbourIndex] ||
						!m_mapControlGrid[neighbourIndex].m_isControlled)
						continue;

					visited[neighbourIndex] = TRUE;
					pending.push_back(neighbourIndex);
				}
			}

			components.push_back(component);
		}

		if (components.size() <= 1)
			return;

		// Find the shortest connection between any two components.
		Int bestTarget = -1;
		std::vector<Int> bestPrevious;
		Int bestDistance = maxConnectionDistance + 1;

		for (Int componentIndex = 0; componentIndex < components.size(); componentIndex++)
		{
			std::vector<Bool> searchVisited(gridSize, FALSE);
			std::vector<Int> searchPrevious(gridSize, -1);
			std::vector<Int> searchDistance(gridSize, 0);
			std::vector<Int> searchPending;

			// Start BFS from every node in this component.
			for (Int i = 0; i < components[componentIndex].size(); i++)
			{
				Int start = components[componentIndex][i];

				searchVisited[start] = TRUE;
				searchPending.push_back(start);
			}

			Int meetingNode = -1;

			for (Int i = 0; i < searchPending.size() && meetingNode < 0; i++)
			{
				Int current = searchPending[i];

				Int x = current % gridWidth;
				Int y = current / gridWidth;

				for (Int direction = 0; direction < 8; direction++)
				{
					Int nx = x + dx[direction];
					Int ny = y + dy[direction];

					if (nx < 0 || nx >= gridWidth ||
							ny < 0 || ny >= gridHeight)
						continue;

					Int neighbourIndex = ny * gridWidth + nx;

					if (searchVisited[neighbourIndex])
						continue;

					Int nextDistance = searchDistance[current] + 1;

					if (nextDistance > maxConnectionDistance)
						continue;

					searchVisited[neighbourIndex] = TRUE;
					searchPrevious[neighbourIndex] = current;
					searchDistance[neighbourIndex] = nextDistance;

					// Found another controlled component.
					if (m_mapControlGrid[neighbourIndex].m_isControlled)
					{
						meetingNode = neighbourIndex;
						break;
					}

					searchPending.push_back(neighbourIndex);
				}
			}

			if (meetingNode >= 0 && searchDistance[meetingNode] < bestDistance)
			{
				bestDistance = searchDistance[meetingNode];
				bestTarget = meetingNode;
				bestPrevious = searchPrevious;
			}
		}

		// No components can be connected within NODE_GAP.
		if (bestTarget < 0)
			return;

		// Reconstruct and mark the connection path.
		Int current = bestTarget;

		while (current >= 0)
		{
			m_mapControlGrid[current].m_isControlled = TRUE;
			current = bestPrevious[current];
		}
	}
}


//------------------------------------------------------------------------------------------------
// traceFlankOutlines - Find the actual flank outlines and put them into a new container.
//------------------------------------------------------------------------------------------------
void TacticModule::traceFlankOutlines(Int gridWidth, Int gridHeight, const std::vector<Int>& borderNodes, std::vector<std::vector<Coord3D>>& paths)
{
	paths.clear();

	if (m_mapControlGrid.empty() || borderNodes.empty() || gridWidth <= 0 || gridHeight <= 0)
		return;

	const Int gridSize = m_mapControlGrid.size();

	// Relative direction priorities:
	// 45° left, straight, 45° right,
	// 90° left, 90° right, 135° left,
	// 135° right, backward.
	const Int turnOrder[8] = { 1, 0, 7, 2, 6, 3, 5, 4 };

	// Mark all border nodes.
	std::vector<Bool> borderMask(gridSize, FALSE);

	for (Int i = 0; i < borderNodes.size(); i++)
		borderMask[borderNodes[i]] = TRUE;

	// Each connection is visited only once.
	std::vector<Bool> visitedEdges(gridSize * 8, FALSE);

	for (Int i = 0; i < borderNodes.size(); i++)
	{
		Int start = borderNodes[i];

		// Try all possible starting directions.
		for (Int startDirection = 0; startDirection < 8; startDirection++)
		{
			if (!isValidFlankStep(start, startDirection, gridWidth, gridHeight, borderMask))
				continue;

			if (visitedEdges[start * 8 + startDirection])
				continue;

			std::vector<Coord3D> path;

			Int current = start;
			Int incomingDirection = -1;

			path.push_back(m_mapControlGrid[start].m_pos);

			for (Int step = 0; step < gridSize * 8; step++)
			{
				Int nextDirection = -1;

				// Determine the next direction.
				if (incomingDirection < 0)
				{
					nextDirection = startDirection;
				}
				else
				{
					for (Int rank = 0; rank < 8; rank++)
					{
						Int direction = (incomingDirection + turnOrder[rank]) % 8;

						if (!isValidFlankStep(
							current, direction,
							gridWidth, gridHeight, borderMask))
							continue;

						if (visitedEdges[current * 8 + direction])
							continue;

						nextDirection = direction;
						break;
					}
				}

				// No further unvisited connection.
				if (nextDirection < 0)
					break;

				Int x = current % gridWidth;
				Int y = current / gridWidth;

				const Int dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
				const Int dy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

				Int nx = x + dx[nextDirection];
				Int ny = y + dy[nextDirection];

				Int next = ny * gridWidth + nx;

				// Mark both directions of the connection.
				visitedEdges[current * 8 + nextDirection] = TRUE;
				visitedEdges[next * 8 + (nextDirection + 4) % 8] = TRUE;

				path.push_back(m_mapControlGrid[next].m_pos);

				// Closed loop.
				if (next == start)
					break;

				current = next;
				incomingDirection = nextDirection;
			}

			if (path.size() >= 2)
				paths.push_back(path);
		}
	}
}

//------------------------------------------------------------------------------------------------
// splitPathsAtImpassableTerrain - Remove nodes that are unreachable.
//------------------------------------------------------------------------------------------------
void TacticModule::splitPathsAtImpassableTerrain(const std::vector<std::vector<Coord3D>>& paths, std::vector<std::vector<Coord3D>>& passablePaths)
{
	passablePaths.clear();
	Pathfinder* pathfinder = TheAI->pathfinder();
	if (!pathfinder)
	{
		passablePaths = paths;
		return;
	}

	for (Int pathIndex = 0; pathIndex < paths.size(); pathIndex++)
	{
		const std::vector<Coord3D>& path = paths[pathIndex];
		if (path.size() < 2) continue;

		Bool closed = path.front().x == path.back().x && path.front().y == path.back().y;
		Int pointCount = path.size() - (closed ? 1 : 0);
		if (pointCount < 2) continue;

		std::vector<Int> impassableEdges;
		Int edgeCount = closed ? pointCount : pointCount - 1;
		for (Int i = 0; i < edgeCount; i++)
		{
			Int next = (i + 1) % pointCount;
			if (!pathfinder->isGroundPathPassable(FALSE, path[i], LAYER_GROUND, path[next], 1))
				impassableEdges.push_back(i);
		}

		if (impassableEdges.empty())
		{
			passablePaths.push_back(path);
			continue;
		}

		if (closed)
		{
			Int start = (impassableEdges[0] + 1) % pointCount;
			std::vector<Coord3D> segment;
			segment.push_back(path[start]);
			for (Int step = 0; step < pointCount; step++)
			{
				Int edge = (start + step) % pointCount;
				Int next = (edge + 1) % pointCount;
				if (pathfinder->isGroundPathPassable(FALSE, path[edge], LAYER_GROUND, path[next], 1))
				{
					segment.push_back(path[next]);
				}
				else
				{
					if (segment.size() > 1) passablePaths.push_back(segment);
					segment.clear();
					if (step + 1 < pointCount) segment.push_back(path[next]);
				}
			}
			if (segment.size() > 1) passablePaths.push_back(segment);
		}
		else
		{
			std::vector<Coord3D> segment;
			segment.push_back(path[0]);
			for (Int i = 0; i < edgeCount; i++)
			{
				Int next = i + 1;
				if (pathfinder->isGroundPathPassable(FALSE, path[i], LAYER_GROUND, path[next], 1))
				{
					segment.push_back(path[next]);
				}
				else
				{
					if (segment.size() > 1) passablePaths.push_back(segment);
					segment.clear();
					segment.push_back(path[next]);
				}
			}
			if (segment.size() > 1) passablePaths.push_back(segment);
		}
	}
}

//------------------------------------------------------------------------------------------------
// isValidFlankStep - See if we can reach that point in our flank.
//------------------------------------------------------------------------------------------------
Bool TacticModule::isValidFlankStep(Int current, Int direction, Int gridWidth, Int gridHeight, const std::vector<Bool>& borderMask)
{
	const Int dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
	const Int dy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

	Int x = current % gridWidth;
	Int y = current / gridWidth;

	Int nx = x + dx[direction];
	Int ny = y + dy[direction];

	if (nx < 0 || nx >= gridWidth ||
		ny < 0 || ny >= gridHeight)
		return FALSE;

	Int neighbour = ny * gridWidth + nx;

	if (!borderMask[neighbour])
		return FALSE;

	// Prevent diagonal corner cutting.
	if (dx[direction] != 0 && dy[direction] != 0)
	{
		Int horizontal = y * gridWidth + nx;
		Int vertical = ny * gridWidth + x;

		if (borderMask[horizontal] || borderMask[vertical])
			return FALSE;
	}

	return TRUE;
}

//------------------------------------------------------------------------------------------------
// getDistanceModifier - Calculate the distance and return a modifier between 0 and 1. Offset because yer can't clip
//------------------------------------------------------------------------------------------------
Real TacticModule::getDistanceModifier(Coord3D sourcePos, Coord3D targetPos, Real value, Real offset)
{
	Real dx = sourcePos.x - targetPos.x;
	Real dy = sourcePos.y - targetPos.y;
	Real dist = sqrtf(dx * dx + dy * dy);

	Real modDist = dist - offset;
	if (modDist < 0.0f)
		modDist = 0.0f;

	if (value - offset <= 0.0f)
		return 0.0f;

	Real modifier = 1.0f - (modDist / value);

	return modifier > 0.0f ? modifier : 0.0f;
}

//------------------------------------------------------------------------------------------------
// addToThreatLevelBuckets - See which bucket the object has to go in.
//------------------------------------------------------------------------------------------------
void TacticModule::addToThreatLevelBuckets(Object* obj, Real& bucketAir, Real& bucketTank, Real& bucketVehicle, Real& bucketInfantry, Real distanceModifier)
{
	if (!obj) return;

	const ThingTemplate* templ = obj->getTemplate();
	if (!templ) return;

	Real modBuildcost = templ->friend_getBuildCost() * distanceModifier;

	if (m_table->hasAnyType(templ, UT_AIR)) {
		bucketAir += modBuildcost;
		return;
	}
	if (m_table->hasAnyType(templ, UT_TANK)) {
		bucketTank += modBuildcost;
		return;
	}
	if (m_table->hasAnyType(templ, UT_VEHICLE)) {
		bucketVehicle += modBuildcost;
		return;
	}
	if (m_table->hasAnyType(templ, UT_INFANTRY)) {
		bucketInfantry += modBuildcost;
		return;
	}
}

//------------------------------------------------------------------------------------------------
// subractFromThreatLevelBuckets - See which bucket to subract the object's build cost from.
//------------------------------------------------------------------------------------------------
void TacticModule::subractFromThreatLevelBuckets(Object* obj, Real& bucketAir, Real& bucketTank, Real& bucketVehicle, Real& bucketInfantry, Real distanceModifier)
{
	if (!obj) return;

	const ThingTemplate* templ = obj->getTemplate();
	if (!templ) return;

	Real modBuildcost = templ->friend_getBuildCost() * distanceModifier;

	if (m_table->hasAnyType(templ, UT_ANTI_AIR)) {
		consumeThreat(bucketAir, modBuildcost);
	}

	if (modBuildcost > 0.0f && m_table->hasAnyType(templ, UT_ANTI_TANK)) {
		consumeThreat(bucketTank, modBuildcost);
	}

	if (modBuildcost > 0.0f && m_table->hasAnyType(templ, UT_ANTI_VEHICLE)) {
		consumeThreat(bucketVehicle, modBuildcost);
	}

	if (modBuildcost > 0.0f && m_table->hasAnyType(templ, UT_ANTI_INFANTRY)) {
		consumeThreat(bucketInfantry, modBuildcost);
	}
}

//------------------------------------------------------------------------------------------------
// consumeThreat - A single operation to subtract the greater from the smaller bucket.
//------------------------------------------------------------------------------------------------
void TacticModule::consumeThreat(Real& bucket, Real& buildCost)
{
	Real usedBucket = (bucket < buildCost) ? bucket : buildCost;

	bucket -= usedBucket;
	buildCost -= usedBucket;
}

//------------------------------------------------------------------------------------------------
// addToDefenseLevel - See how much our object contributes to the defense level.
//------------------------------------------------------------------------------------------------
void TacticModule::addToDefenseLevel(Object* obj, Real& defenseLevel, Real distanceModifier)
{
	if (!obj->isKindOf(KINDOF_FS_BASE_DEFENSE))
		return;

	const ThingTemplate* templ = obj->getTemplate();
	if (!templ) return;

	Real modBuildcost = templ->friend_getBuildCost() * distanceModifier;
	defenseLevel += modBuildcost;
}

//------------------------------------------------------------------------------------------------
// init - Default values for the entire table.
//------------------------------------------------------------------------------------------------
void UnitTypeTable::init()
{
	add(TheThingFactory->findTemplate("AmericaInfantryRanger"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("AirF_AmericaInfantryRanger"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Lazr_AmericaInfantryRanger"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("SupW_AmericaInfantryRanger"), UT_INFANTRY | UT_ANTI_INFANTRY);

	add(TheThingFactory->findTemplate("AmericaInfantryMissileDefender"), UT_INFANTRY | UT_ANTI_TANK | UT_ANTI_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("AirF_AmericaInfantryMissileDefender"), UT_INFANTRY | UT_ANTI_TANK | UT_ANTI_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Lazr_AmericaInfantryMissileDefender"), UT_INFANTRY | UT_ANTI_TANK | UT_ANTI_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("SupW_AmericaInfantryMissileDefender"), UT_INFANTRY | UT_ANTI_TANK | UT_ANTI_VEHICLE | UT_ANTI_AIR);

	add(TheThingFactory->findTemplate("AmericaInfantryPathfinder"), UT_INFANTRY | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("AirF_AmericaInfantryPathfinder"), UT_INFANTRY | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Lazr_AmericaInfantryPathfinder"), UT_INFANTRY | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("SupW_AmericaInfantryPathfinder"), UT_INFANTRY | UT_ANTI_AIR);

	add(TheThingFactory->findTemplate("AmericaInfantryColonelBurton"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("AirF_AmericaInfantryColonelBurton"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Lazr_AmericaInfantryColonelBurton"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("SupW_AmericaInfantryColonelBurton"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);

	add(TheThingFactory->findTemplate("AmericaJetAurora"), UT_AIR | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("AirF_AmericaJetAurora"), UT_AIR | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Lazr_AmericaJetAurora"), UT_AIR | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("SupW_AmericaJetAurora"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("AmericaJetRaptor"), UT_AIR | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("AirF_AmericaJetRaptor"), UT_AIR | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Lazr_AmericaJetRaptor"), UT_AIR | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("SupW_AmericaJetRaptor"), UT_AIR | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("AmericaJetStealthFighter"), UT_AIR | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("AirF_AmericaJetStealthFighter"), UT_AIR | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Lazr_AmericaJetStealthFighter"), UT_AIR | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("SupW_AmericaJetStealthFighter"), UT_AIR | UT_ANTI_VEHICLE);

	add(TheThingFactory->findTemplate("AmericaTankAvenger"), UT_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("AirF_AmericaTankAvenger"), UT_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Lazr_AmericaTankAvenger"), UT_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("SupW_AmericaTankAvenger"), UT_VEHICLE | UT_ANTI_AIR);

	add(TheThingFactory->findTemplate("AmericaTankCrusader"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Lazr_AmericaTankCrusader"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("AmericaTankMicrowave"), UT_TANK);
	add(TheThingFactory->findTemplate("AirF_AmericaTankMicrowave"), UT_TANK);
	add(TheThingFactory->findTemplate("Lazr_AmericaTankMicrowave"), UT_TANK);
	add(TheThingFactory->findTemplate("SupW_AmericaTankMicrowave"), UT_TANK);

	add(TheThingFactory->findTemplate("AmericaTankPaladin"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("AmericaVehicleComanche"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("AirF_AmericaVehicleComanche"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Lazr_AmericaVehicleComanche"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("SupW_AmericaVehicleComanche"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("AmericaVehicleHumvee"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("AirF_AmericaVehicleHumvee"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Lazr_AmericaVehicleHumvee"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("SupW_AmericaVehicleHumvee"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);

	add(TheThingFactory->findTemplate("AmericaVehicleMedic"), UT_VEHICLE);
	add(TheThingFactory->findTemplate("AirF_AmericaVehicleMedic"), UT_VEHICLE);
	add(TheThingFactory->findTemplate("Lazr_AmericaVehicleMedic"), UT_VEHICLE);
	add(TheThingFactory->findTemplate("SupW_AmericaVehicleMedic"), UT_VEHICLE);

	add(TheThingFactory->findTemplate("AmericaVehicleSentryDrone"), UT_VEHICLE | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("AirF_AmericaVehicleSentryDrone"), UT_VEHICLE | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Lazr_AmericaVehicleSentryDrone"), UT_VEHICLE | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("SupW_AmericaVehicleSentryDrone"), UT_VEHICLE | UT_ANTI_INFANTRY);

	add(TheThingFactory->findTemplate("AmericaVehicleTomahawk"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("AirF_AmericaVehicleTomahawk"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("SupW_AmericaVehicleTomahawk"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("ChinaInfantryRedguard"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Infa_ChinaInfantryMiniGunner"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Nuke_ChinaInfantryRedguard"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Tank_ChinaInfantryRedguard"), UT_INFANTRY | UT_ANTI_INFANTRY);

	add(TheThingFactory->findTemplate("ChinaInfantryTankHunter"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Infa_ChinaInfantryTankHunter"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Nuke_ChinaInfantryTankHunter"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Tank_ChinaInfantryTankHunter"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK | UT_ANTI_AIR);

	add(TheThingFactory->findTemplate("ChinaInfantryBlackLotus"), UT_INFANTRY);
	add(TheThingFactory->findTemplate("Infa_ChinaInfantryBlackLotus"), UT_INFANTRY);
	add(TheThingFactory->findTemplate("Nuke_ChinaInfantryBlackLotus"), UT_INFANTRY);
	add(TheThingFactory->findTemplate("Tank_ChinaInfantryBlackLotus"), UT_INFANTRY);

	add(TheThingFactory->findTemplate("ChinaJetMIG"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Infa_ChinaJetMIG"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Nuke_ChinaJetMIG"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Tank_ChinaJetMIG"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("ChinaTankBattleMaster"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Nuke_ChinaTankBattleMaster"), UT_TANK | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Tank_ChinaTankBattleMaster"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("ChinaTankDragon"), UT_TANK | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Infa_ChinaTankDragon"), UT_TANK | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Nuke_ChinaTankDragon"), UT_TANK | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Tank_ChinaTankDragon"), UT_TANK | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);

	add(TheThingFactory->findTemplate("ChinaTankECM"), UT_TANK);
	add(TheThingFactory->findTemplate("Infa_ChinaTankECM"), UT_TANK);
	add(TheThingFactory->findTemplate("Nuke_ChinaTankECM"), UT_TANK);
	add(TheThingFactory->findTemplate("Tank_ChinaTankECM"), UT_TANK);

	add(TheThingFactory->findTemplate("ChinaTankGattling"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Nuke_ChinaTankGattling"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Tank_ChinaTankGattling"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_AIR);

	add(TheThingFactory->findTemplate("ChinaTankOverlord"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Nuke_ChinaTankOverlord"), UT_TANK | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Tank_ChinaTankOverlord"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("ChinaVehicleHelix"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Infa_ChinaVehicleHelix"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Nuke_ChinaVehicleHelix"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Tank_ChinaVehicleHelix"), UT_AIR | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);

	add(TheThingFactory->findTemplate("ChinaVehicleInfernoCannon"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Infa_ChinaVehicleInfernoCannon"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Nuke_ChinaVehicleInfernoCannon"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);

	add(TheThingFactory->findTemplate("ChinaVehicleListeningOutpost"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Infa_ChinaVehicleListeningOutpost"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Nuke_ChinaVehicleListeningOutpost"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Tank_ChinaVehicleListeningOutpost"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("ChinaVehicleNukeLauncher"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Infa_ChinaVehicleNukeLauncher"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Nuke_ChinaVehicleNukeLauncher"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("ChinaVehicleTroopCrawler"), UT_VEHICLE | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Infa_ChinaVehicleTroopCrawler"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Nuke_ChinaVehicleTroopCrawler"), UT_VEHICLE | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Tank_ChinaVehicleTroopCrawler"), UT_VEHICLE | UT_ANTI_INFANTRY);

	add(TheThingFactory->findTemplate("GLAInfantryRebel"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Chem_GLAInfantryRebel"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Demo_GLAInfantryRebel"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Slth_GLAInfantryRebel"), UT_INFANTRY | UT_ANTI_INFANTRY);

	add(TheThingFactory->findTemplate("GLAInfantryHijacker"), UT_INFANTRY);
	add(TheThingFactory->findTemplate("Slth_GLAInfantryHijacker"), UT_INFANTRY);

	add(TheThingFactory->findTemplate("GLAInfantryJarmenKell"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Chem_GLAInfantryJarmenKell"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Demo_GLAInfantryJarmenKell"), UT_INFANTRY | UT_ANTI_INFANTRY);
	add(TheThingFactory->findTemplate("Slth_GLAInfantryJarmenKell"), UT_INFANTRY | UT_ANTI_INFANTRY);

	add(TheThingFactory->findTemplate("GLAInfantryTerrorist"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Chem_GLAInfantryTerrorist"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Demo_GLAInfantryTerrorist"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Slth_GLAInfantryTerrorist"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("GLAInfantryTunnelDefender"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Chem_GLAInfantryTunnelDefender"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Demo_GLAInfantryTunnelDefender"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Slth_GLAInfantryTunnelDefender"), UT_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK | UT_ANTI_AIR);

	add(TheThingFactory->findTemplate("GLAInfantryAngryMobNexus"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Chem_GLAInfantryAngryMobNexus"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Demo_GLAInfantryAngryMobNexus"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Slth_GLAInfantryAngryMobNexus"), UT_INFANTRY | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("GLATankMarauder"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Chem_GLATankMarauder"), UT_TANK | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Demo_GLATankMarauder"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("GLATankScorpion"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Chem_GLATankScorpion"), UT_TANK | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Demo_GLATankScorpion"), UT_TANK | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("GLAVehicleBattleBus"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Chem_GLAVehicleBattleBus"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Demo_GLAVehicleBattleBus"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Slth_GLAVehicleBattleBus"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("GLAVehicleBombTruck"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Chem_GLAVehicleBombTruck"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Demo_GLAVehicleBombTruck"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Slth_GLAVehicleBombTruck"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("GLAVehicleCombatBike"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Chem_GLAVehicleCombatBike"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Demo_GLAVehicleCombatBike"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Slth_GLAVehicleCombatBike"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);

	add(TheThingFactory->findTemplate("GLAVehicleQuadCannon"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Chem_GLAVehicleQuadCannon"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Demo_GLAVehicleQuadCannon"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_AIR);
	add(TheThingFactory->findTemplate("Slth_GLAVehicleQuadCannon"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_AIR);

	add(TheThingFactory->findTemplate("GLAVehicleRocketBuggy"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Chem_GLAVehicleRocketBuggy"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Demo_GLAVehicleRocketBuggy"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Slth_GLAVehicleRocketBuggy"), UT_VEHICLE | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("GLAVehicleScudLauncher"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);
	add(TheThingFactory->findTemplate("Chem_GLAVehicleScudLauncher"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Demo_GLAVehicleScudLauncher"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE | UT_ANTI_TANK);

	add(TheThingFactory->findTemplate("GLAVehicleTechnical"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Chem_GLAVehicleTechnical"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Demo_GLAVehicleTechnical"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Slth_GLAVehicleTechnical"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);

	add(TheThingFactory->findTemplate("GLAVehicleToxinTruck"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Chem_GLAVehicleToxinTruck"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Demo_GLAVehicleToxinTruck"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
	add(TheThingFactory->findTemplate("Slth_GLAVehicleToxinTruck"), UT_VEHICLE | UT_ANTI_INFANTRY | UT_ANTI_VEHICLE);
}

//------------------------------------------------------------------------------------------------
// add - Add an entry if it doesn't already exist. If it exists, modify it.
//------------------------------------------------------------------------------------------------
void UnitTypeTable::add(const ThingTemplate* templ, UnitTypeMask mask)
{
	if (!templ)
		return;

	std::map<const ThingTemplate*, UnitTypeMask>::iterator it = m_table.find(templ);

	// this entry doesn't yet exist
	if (it == m_table.end()) {
		m_table.insert(std::pair<const ThingTemplate*, UnitTypeMask>(templ, mask));
	}
	// an entry exists
	else {
		it->second |= mask;
	}
}

//------------------------------------------------------------------------------------------------
// hasAnyType - Check whether the given mask is part of the bitset.
//------------------------------------------------------------------------------------------------
Bool UnitTypeTable::hasAnyType(const ThingTemplate* templ, UnitTypeMask mask)
{
	if (!templ)
		return FALSE;

	std::map<const ThingTemplate*, UnitTypeMask>::iterator it = m_table.find(templ);
	if (it == m_table.end())
		return FALSE;

	return (it->second & mask) != 0;
}


/// -------------------------------------------------------------------------------------

namespace
{

}
