/**
 * DStarLite.
 *
 * Based on "Improved Fast Replanning for Robot Navigation in Unknown Terrain" by
 * Sven Koenig and Maxim Likhachev
 *
 * Figure 6: D* Lite: Final Version (optimized verion).
 *
 * @package		DStarLite
 * @author		Aaron Zampaglione <azampagl@gmail.com>
 * @copyright	Copyright (C) 2011 Aaron Zampaglione
 * @license		MIT
 */
#include "planner.h"

/*
 * @var  static const double  max steps before assuming no solution possible
 */
const double Planner::MAX_STEPS = 1000000;

/**
 * Constructor.
 *
 * @param  Map*         map
 * @param  Map::Cell*   start cell
 * @param  Map::Cell*   goal cell
 */
Planner::Planner(Map* map, Map::Cell* start, Map::Cell* goal)
{
	// Clear lists
	_open_list.clear();
	_open_hash.clear();
	_path.clear();
	
	_km = 0;

	_map = map;
	_start = start;
	_goal = goal;
	_last = _start;

	_rhs(_goal, 0.0);

	_list_insert(_goal, _k(_goal));
}

/**
 * Deconstructor.
 */
Planner::~Planner()
{
}

/**
 * Returns the generated path.
 *
 * @return  list<Map::Cell*>
 */
list<Map::Cell*> Planner::path()
{
	return _path;
}

/**
 * Gets/Sets a new goal.
 *
 * @param   Map::Cell* [optional]   goal
 * @return  Map::Cell*              new goal
 */
Map::Cell* Planner::goal(Map::Cell* u)
{
	if (u == NULL)
		return _goal;

	// Hack implementation
	_goal = u;

	return _goal;
}

/**
 * Replans the path.
 *
 * @return  bool   solution found
 */
bool Planner::replan()
{
	_path.clear();
	
	bool result = _compute();
	
	// Couldn't find a solution
	if ( ! result)
	  return false;

	Map::Cell** nbrs = NULL;
	Map::Cell* min_cell = NULL;
	double min_cost, tmp_cost, tmp_g = Math::INFINITY;

	Map::Cell* current = _start;
	_path.push_back(current);

	// Follow the path with the least cost until goal is reached
	while (current != _goal)
	{
		if (current == NULL || Math::equals(_g(current), Math::INFINITY))
			return false;

		current = _min_succ(current);

		_path.push_back(current);
	}

	return true;
}

/**
 * Gets/Sets start.
 *
 * @param   Map::Cell* [optional]   new start
 * @return  Map::Cell*              start
 */
Map::Cell* Planner::start(Map::Cell* u)
{
	if (u == NULL)
		return _start;

	_start = u;

	return _start;
}

/**
 * Update map.
 *
 * @param   Map::Cell*   cell to update
 * @param   double       new cost of the cell
 * @return  void
 */
void Planner::update(Map::Cell* u, double cost)
{
	if (u == _goal)
		return;

	// Update km
	_km += _h(_last, _start);
	_last = _start;

	_cell(u);

	double cost_old = u->cost;
	u->cost = cost;

	_update(u);
}

/**
 * Generates a cell.
 *
 * @param   Map::Cell*
 * @return  void
 */
void Planner::_cell(Map::Cell* u)
{
	if (_cell_hash.find(u) != _cell_hash.end())
		return;
	
	double h = Math::INFINITY;
	_cell_hash[u] = pair<double,double>(h, h);
}

/**
 * Computes shortest path.
 *
 * @return  bool   successful
 */
bool Planner::_compute()
{
	if (_open_list.empty())
		return false;

	KeyCompare key_compare;

	Map::Cell* u;
	Map::Cell** nbrs;

	pair<double,double> k_old;
	pair<double,double> k_new;

	int attempts = 0;

	while ( ! _open_list.empty() && key_compare(_open_list.begin()->first, _k(_start)) || ! Math::equals(_rhs(_start), _g(_start)))
	{
		// Reached max steps, quit
		if (++attempts > Planner::MAX_STEPS)
			return false;

		u = _open_list.begin()->second;
		k_old = _open_list.begin()->first;
		k_new = _k(u);
		
		if (key_compare(k_old, k_new))
		{
			_list_update(u, k_new);
		}
		else if (Math::greater(_g(u), _rhs(u)))
		{
			_g(u, _rhs(u));

			_list_remove(u);

			nbrs = u->nbrs();

			for (unsigned int i = 0; i < Map::Cell::NUM_NBRS; i++)
			{
				if (nbrs[i] != NULL)
				{
					if (nbrs[i] != _goal)
					{
						_rhs(nbrs[i], min(_rhs(nbrs[i]), _cost(nbrs[i], u) + _g(u)));
					}

					_update(nbrs[i]);
				}
			}
		}
		else
		{
			double g_old = _g(u);
			_g(u, Math::INFINITY);

			nbrs = u->nbrs();

			double min_cost, tmp_cost, tmp_g;
			min_cost = tmp_cost = tmp_g = Math::INFINITY;

			if (u != _goal)
			{
				tmp_g = _g(u);

				if (Math::equals(tmp_g, Math::INFINITY))
					break;

				for (unsigned int i = 0; i < Map::Cell::NUM_NBRS; i++)
				{
					if (nbrs[i] != NULL)
					{
						tmp_cost = _cost(u, nbrs[i]);

						if (Math::equals(tmp_cost, Math::INFINITY))
							continue;

						tmp_cost += tmp_g;

						if (Math::less(tmp_cost, min_cost))
						{
							min_cost = tmp_cost;
						}
					}
				}

				_rhs(u, min);
			}

			_update(u);

			for (unsigned int i = 0; i < Map::Cell::NUM_NBRS; i++)
			{
				if (nbrs[i] != NULL)
				{
					_update(nbrs[i]);
				}
			}
		}
	}

	return true;
}

/**
 * Calculates the cost from one cell to another cell.
 * 
 * @param   Map::Cell*   cell a
 * @param   Map::Cell*   cell b
 * @return  double       cost between a and b
 */
double Planner::_cost(Map::Cell* a, Map::Cell* b)
{
	if (a->cost == Map::Cell::COST_UNWALKABLE || b->cost == Map::Cell::COST_UNWALKABLE)
		return Map::Cell::COST_UNWALKABLE;

	unsigned int dx = labs(a->x() - b->x());
	unsigned int dy = labs(a->y() - b->y());
	double scale = 1.0;

	if ((dx + dy) > 1)
	{
		scale = Math::SQRT2;
	}

	return scale * ((a->cost + b->cost) / 2);
}

/**
 * Gets/Sets g value for a cell.
 * 
 * @param   Map::Cell*          cell to retrieve/update
 * @param   double [optional]   new g value
 * @return  double              g value 
 */
double Planner::_g(Map::Cell* u, double value)
{
	_cell(u);
	pair<double,double>* g_rhs = &_cell_hash[u];

	if (value != DBL_MIN)
	{
		g_rhs->first = value;
	}

	return g_rhs->first;
}

/**
 * Calculates heuristic between two cells (manhattan distance).
 *
 * @param   Map::Cell*   cell a
 * @param   Map::Cell*   cell b
 * @return  double       heuristic value
 */
double Planner::_h(Map::Cell* a, Map::Cell* b)
{
	double min = labs(a->x() - b->x());
	double max = labs(a->y() - b->y());
	
	if (min > max)
	{
		double tmp = min;
		min = max;
		max = tmp;
	}
	
	return ((Math::SQRT2 - 1.0) * min + max);
}

/**
 * Calculates key value for cell.
 *
 * @param   Map::Cell*            cell to calculate for
 * @return  pair<double,double>   key value
 */
pair<double,double> Planner::_k(Map::Cell* u)
{
	double g = _g(u);
	double rhs = _rhs(u);
	double min = Math::less(g, rhs) ? g : rhs;
	return pair<double,double>((min + _h(_start, u) + _km), min);
}

/**
 * Inserts cell into open list.
 *
 * @param   Map::Cell*            cell to insert
 * @param   pair<double,double>   key vakue for the cell
 * @return  void
 */
void Planner::_list_insert(Map::Cell* u, pair<double,double> k)
{
	OL::iterator pos = _open_list.insert(OL_PAIR(k, u));
	_open_hash[u] = pos;
}

/**
 * Removes cell from the open list.
 *
 * @param   Map::Cell*   cell to remove
 * @return  void
 */
void Planner::_list_remove(Map::Cell* u)
{
	_open_list.erase(_open_hash[u]);
	_open_hash.erase(_open_hash.find(u));
}

/**
 * Updates cell in the open list.
 *
 * @param   Map::Cell*
 * @param   pair<double,double>
 * @return  void
 */
void Planner::_list_update(Map::Cell* u, pair<double,double> k)
{
	OL::iterator pos1 = _open_hash[u];
	OL::iterator pos2 = pos1;

	if (pos1 == _open_list.end())
	{
		pos2 = _open_list.end();
	}
	else
	{
		pos2++;
	}

	_open_list.erase(pos1);
	_open_hash[u] = _open_list.insert(pos2, OL_PAIR(k, u));
}

/**
 * Finds the minimum successor cell.
 *
 * @param   Map::Cell*   root
 * @return  Map::Cell*   successor
 */
Map::Cell* Planner::_min_succ(Map::Cell* u)
{
	Map::Cell** nbrs = u->nbrs();

	double tmp_cost, tmp_g;
	
	Map::Cell* min_cell = NULL;
	double min_cost = Math::INFINITY;

	for (unsigned int i = 0; i < Map::Cell::NUM_NBRS; i++)
	{
		if (nbrs[i] != NULL)
		{
			tmp_cost = _cost(u, nbrs[i]);
			tmp_g = _g(nbrs[i]);

			if (Math::equals(tmp_cost, Math::INFINITY) || Math::equals(tmp_g, Math::INFINITY))
				continue;
			
			tmp_cost += tmp_g;

			if (Math::less(tmp_cost, min_cost))
			{
				min_cost = tmp_cost;
				min_cell = nbrs[i];
			}
		}
	}

	return min_cell;
}

/**
 * Gets/Sets rhs value for a cell.
 * 
 * @param   Map::Cell*          cell to retrieve/update
 * @param   double [optional]   new rhs value
 * @return  double              rhs value
 */
double Planner::_rhs(Map::Cell* u, double value)
{
	if (u == _goal)
		return 0;

	_cell(u);
	pair<double,double>* g_rhs = &_cell_hash[u];

	if (value != DBL_MIN)
	{
		g_rhs->second = value;
	}
	
	return g_rhs->second;
}

/**
 * Updates cell.
 *
 * @param   Map::Cell*   cell to update
 * @return  void
 */
void Planner::_update(Map::Cell* u)
{
	bool diff = ! Math::equals(_g(u), _rhs(u));
	bool exists = (_open_hash.find(u) != _open_hash.end());

	if (diff && exists)
	{
		_list_update(u, _k(u));
	}
	else if (diff && ! exists)
	{
		_list_insert(u, _k(u));
	}
	else if ( ! diff && exists)
	{
		_list_remove(u);
	}
}

/**
 * Key compare function.
 */
bool Planner::KeyCompare::operator()(const pair<double,double>& p1, const pair<double,double>& p2) const
{
	if (Math::less(p1.first, p2.first))				return true;
	else if (Math::greater(p1.first, p2.first))		return false;
	else if (Math::less(p1.second,  p2.second))		return true;
	else if (Math::greater(p1.second, p2.second))	return false;
													return false;
}