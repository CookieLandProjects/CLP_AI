////////////////////////////////////////////////////////////////////////////////
//																																						//
//  CLP_AI Improved AI Project																								//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// TacticModule.h
// Tactical evaluation system for the AIPlayer
// Author: -TanSo-, November 2026

#pragma once

#include "Common/GameMemory.h"
#include "Common/Snapshot.h"

#define MAX_DEFENSE_DISTANCE 400.0f
#define MAX_DECTECTION_DISTANCE 1000.0f
#define BUILDCOST_NORMALIZATION 1500.0f
#define SUPPLY_NORMALIZATION 20000.0f
#define OIL_DERRICK_VALUE 1000.0f
#define SUPPLY_BOX_VALUE (float)TheGlobalData->m_baseValuePerSupplyBox

#define MAP_CONTROL_GRID_INTERVAL 200.0f
#define MAP_CONTROL_CAPTURE_RADIUS 300.0f
#define NODE_GAP 2

class Player;

class TacticModule;
class UnitTypeTable;

typedef unsigned int UnitTypeMask;		// Bit-Flag for UnitTypes
enum UnitType {
	UT_NONE = 0,
	UT_INFANTRY =				1 << 0,
	UT_VEHICLE =				1 << 1,
	UT_TANK =						1 << 2,
	UT_AIR =						1 << 3,
	UT_ANTI_INFANTRY =	1 << 4,
	UT_ANTI_TANK =			1 << 5,
	UT_ANTI_VEHICLE =		1 << 6,
	UT_ANTI_AIR =				1 << 7
};

// A node on the path of our FlankInfo.
struct MapControlNode
{
	Coord3D m_pos;								// The position of the node.
	Bool		m_isControlled;				// This node belongs to our territory.
	Real		m_defenseLevel;				// 0 -> 1. How tightly packed defenses are around me.
	Real		m_threatLevel;				// 0 -> 1. The enemy activity around this position.
};

// Info about the resources we possess. Various supply source types, TechOilDerrick and TechOilRefinery for content.
struct ResourceInfo
{
	ObjectID	m_id;								// The object we are tied to.
	Real			m_threatLevel;			// 0 -> 1. The enemy activity around this object.
	Real			m_defenseLevel;			// 0 -> 1. How well we are protected.
	Real			m_strategicValue;		// 0 -> 1. Does it matter if we surrendered this object to the enemy?
};

// Info about the flanks we have. Each flank handles a vector of coordinates for its path.
struct FlankInfo
{
	std::vector<MapControlNode> m_path;	// The line this flank represents.

	Real m_threatLevel;						// The average of each node's threat level (density).
	Real m_defenseLevel;					// The average of each node's defense level (density).
};

//------------------------------------------------------------------------------------------------
// TacticModule - This will make decision making for AIs easier.
// NOTE NOTE NOTE: INFORMATION GATHERING ONLY!!!
// THIS SHOULD NEVER EXECUTE ACTIONS. SCRIPT ENGINE IS DA WAE!!!
//------------------------------------------------------------------------------------------------
class TacticModule : //public MemoryPoolObject,
										 public Snapshot
{
	//MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TacticModule, "TacticModule")

public:
	TacticModule(Player* p);
	~TacticModule() { m_currentFlanks.clear();  m_ownedResources.clear(); delete m_table; }

	void update();

	// Getters
	Real getRawStrength() const { return m_rawStrength; }
	Real getRelativeStrength() const { return m_relativeStrength; }
	Real getRawTeamStrength() const { return m_rawTeamStrength; }
	Real getRelativeTeamStrength() const { return m_relativeTeamStrength; }

	Real getRawEconomy() const { return m_rawEconomy; }
	Real getRelativeEconomy() const { return m_relativeEconomy; }
	Real getRawTeamEconomy() const { return m_rawTeamEconomy; }
	Real getRelativeTeamEconomy() const { return m_relativeTeamEconomy; }

	Real getMapControl() const { return m_mapControl; }
	Real getTeamMapControl() const { return m_teamMapControl; }

	Bool getTacticInfoEntry(ResourceInfo &info, Object* resource);

	// Offensive stuff
	Bool shouldAttack();
	Bool shouldHarass(); // is the enemy spread out thin so it makes sense to poke around?

	// Economy-related stuff
	Bool shouldExpand();
	Bool shouldBuildByResource(Object* resource);
	Bool shouldContestResource(Object* resource);
	Bool shouldProtectResource(Object* resource);

	// Defensive stuff
	Bool shouldTurtle();
	Bool shouldRegroup();
	Bool shouldRetreat();

protected:
	// Snapshot methods
	virtual void xfer(Xfer* xfer) override;
	virtual void crc(Xfer* xfer) override;
	virtual void loadPostProcess() override;

	// Update methods
	void updateStrengthAndEconomy();										// Recalc strength and eco parameters.
	void updateUnitTypeTable();													// See if units need to be modified after upgrade purchases.
	void updateMapControl();														// See how our map control has changed since the last time.
	void processMapControl();														// Figure out what to do with the values of each node.
	void updateResourceInfo();													// Check which resource to throw out, keep, or add.
	void processResourceInfo();													// Calculate the info for each resource.
	void updateFlankInfo();															// Check how flanks have shifted.
	void processFlankInfo();														// Calculate the info for each flank.

	// Helper methods for update
	Real getDistanceModifier(Coord3D sourcePos, Coord3D targetPos, Real value, Real offset);
	void addToThreatLevelBuckets(Object* obj, Real &bucketAir, Real &bucketTank, Real &bucketVehicle, Real &bucketInfantry, Real distanceModifier);
	void subractFromThreatLevelBuckets(Object* obj, Real& bucketAir, Real& bucketTank, Real& bucketVehicle, Real& bucketInfantry, Real distanceModifier);
	void consumeThreat(Real& bucket, Real& buildCost);
	void addToDefenseLevel(Object* obj, Real& defenseLevel, Real distanceModifier);

	Bool isPointControlled(Player* p, Coord3D point);

	void connectFlankGridComponents(Int gridWidth, Int gridHeight);
	void traceFlankOutlines(Int gridWidth, Int gridHeight, const std::vector<Int>& borderNodes, std::vector<std::vector<Coord3D>>& paths);
	void splitPathsAtImpassableTerrain(const std::vector<std::vector<Coord3D>>& paths, std::vector<std::vector<Coord3D>>& passablePaths);
	Bool isValidFlankStep(Int current, Int direction, Int gridWidth, Int gridHeight, const std::vector<Bool>& borderMask);

private:
	Player* m_player;																		// The player we are tied to.
	UnitTypeTable* m_table;															// The table we use to compare stuff against.

	Real m_rawStrength;																	// Joint build cost of all attacking units the player posesses. Unit health modifies the actual worth
	Real m_relativeStrength;														// m_rawStrength put into context with the other players.
	Real m_rawTeamStrength;															// Joint build cost of all attacking units the team posesses. Unit health modifies the actual worth
	Real m_relativeTeamStrength;												// m_rawTeamStrength put into context with the other team(s)

	Real m_rawEconomy;
	Real m_relativeEconomy;
	Real m_rawTeamEconomy;
	Real m_relativeTeamEconomy;

	Real m_mapControl;																	// 0 -> 1. What does my mapcontrol look like? 1.00 = 100%
	Real m_teamMapControl;															// 0 -> 1. What does the team's mapcontrol look like? 1.00 = 100%

	std::vector<MapControlNode> m_mapControlGrid;				// Storage for each node in the grid. Reduces isPointControlled() usage.
	std::vector<ResourceInfo> m_ownedResources;					// Info about all of our owned supplies & oils
	std::vector<FlankInfo> m_currentFlanks;							// Info about our current flanks, how well they are protected etc.

	std::vector<Object*> m_debugUnits;
};

//------------------------------------------------------------------------------------------------
// UnitTypeTable - A table about counter capabilities of units.
// Since there are ways for units to change their counter capabilities, i.e. the humvee,
// each AIPlayer receives their own table, depending on whether the upgrade has been purchased...
//------------------------------------------------------------------------------------------------
class UnitTypeTable
{
public:
	UnitTypeTable() { init(); }
	~UnitTypeTable() { m_table.clear(); }
	void init();
	void add(const ThingTemplate* templ, UnitTypeMask mask);

	Bool hasAnyType(const ThingTemplate* templ, UnitTypeMask mask);
private:
	std::map<const ThingTemplate*, UnitTypeMask> m_table;
};
