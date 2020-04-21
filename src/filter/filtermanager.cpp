/*
 * This file is part of ATSDB.
 *
 * ATSDB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ATSDB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with ATSDB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "filtermanager.h"

#include "atsdb.h"
#include "configurationmanager.h"
#include "datasourcesfilter.h"
#include "dbconnection.h"
#include "dbfilter.h"
#include "dbinterface.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "dbovariable.h"
#include "filtermanagerwidget.h"
#include "logger.h"
#include "viewpoint.h"

#include "json.hpp"

using namespace std;
using namespace nlohmann;

FilterManager::FilterManager(const std::string& class_id, const std::string& instance_id,
                             ATSDB* atsdb)
    : Configurable(class_id, instance_id, atsdb, "filter.json")
{
    logdbg << "FilterManager: constructor";

    registerParameter("db_id", &db_id_, "");
}

FilterManager::~FilterManager()
{
    for (unsigned int cnt = 0; cnt < filters_.size(); cnt++)
        delete filters_.at(cnt);
    filters_.clear();

    if (widget_)
    {
        delete widget_;
        widget_ = nullptr;
    }
}

void FilterManager::generateSubConfigurable(const std::string& class_id,
                                            const std::string& instance_id)
{
    if (class_id == "DBFilter")
    {
        if (hasSubConfigurable(class_id, instance_id))
        {
            logerr << "FilterManager: generateSubConfigurable: filter " << instance_id
                   << " already present";
            return;
        }

        DBFilter* filter = new DBFilter(class_id, instance_id, this);
        filters_.push_back(filter);
    }
    else if (class_id.compare("DataSourcesFilter") == 0)
    {
        try
        {
            if (hasSubConfigurable(class_id, instance_id))
            {
                logerr << "FilterManager: generateSubConfigurable: data sources filter "
                       << instance_id << " already present";
                return;
            }
            std::string dbo_name = configuration()
                                       .getSubConfiguration(class_id, instance_id)
                                       .getParameterConfigValueString("dbo_name");

            if (!ATSDB::instance().objectManager().existsObject(dbo_name))
            {
                loginf << "FilterManager: generateSubConfigurable: disabling data sources filter "
                       << instance_id << " because of non-existing dbobject '" << dbo_name << "'";
                return;
            }

            DBObject& object = ATSDB::instance().objectManager().object(dbo_name);

            if (!object.hasCurrentDataSourceDefinition())
            {
                loginf << "FilterManager: generateSubConfigurable: disabling data sources filter "
                       << instance_id << " because of missing data source definition";
                return;
            }

            if (!object.hasDataSources())
            {
                loginf << "FilterManager: generateSubConfigurable: disabling data sources filter "
                       << instance_id << " because of missing data sources";
                return;
            }

            if (!object.existsInDB())
            {
                loginf << "FilterManager: generateSubConfigurable: disabling data sources filter "
                       << instance_id << " because of empty dbobject '" << dbo_name << "'";
                return;
            }

            DataSourcesFilter* filter = new DataSourcesFilter(class_id, instance_id, this);
            if (filter->disabled())
            {
                loginf << "FilterManager: generateSubConfigurable: deleting disabled data source "
                          "filter for object "
                       << filter->dbObjectName();
                // removeChildConfigurable (*filter);
                // configuration_.removeSubConfiguration(class_id, instance_id);
                delete filter;
            }
            else
                filters_.push_back(filter);
        }
        catch (const std::exception& e)
        {
            loginf << "FilterManager: generateSubConfigurable: data source filter exception '"
                   << e.what() << "', deleting";
            configuration().removeSubConfiguration(class_id, instance_id);
        }
    }
    else
        throw std::runtime_error("FilterManager: generateSubConfigurable: unknown class_id " +
                                 class_id);
}
void FilterManager::checkSubConfigurables()
{
    // watch those sensors
    for (auto& obj_it : ATSDB::instance().objectManager())
    {
        if (!obj_it.second->hasCurrentDataSourceDefinition() || !obj_it.second->hasDataSources() ||
            !obj_it.second->existsInDB())
            continue;

        bool exists = false;
        for (auto fil_it : filters_)
        {
            DataSourcesFilter* sensor_filter = dynamic_cast<DataSourcesFilter*>(fil_it);
            if (sensor_filter && sensor_filter->dbObjectName() == obj_it.first)
            {
                exists = true;
                break;
            }
        }

        if (exists)
            continue;

        loginf << "FilterManager: checkSubConfigurables: generating sensor filter for "
               << obj_it.first;

        std::string instance_id = obj_it.second->name() + "DataSources";
        unsigned int cnt = 0;

        while (
            configuration().hasSubConfiguration("DataSourcesFilter", instance_id + to_string(cnt)))
            ++cnt;

        instance_id += to_string(cnt);

        Configuration& ds_filter_configuration =
            addNewSubConfiguration("DataSourcesFilter", instance_id);

        ds_filter_configuration.addParameterString("dbo_name", obj_it.first);
        generateSubConfigurable("DataSourcesFilter", instance_id);
    }
}

std::string FilterManager::getSQLCondition(const std::string& dbo_name,
                                           std::vector<DBOVariable*>& filtered_variables)
{
    assert(ATSDB::instance().objectManager().object(dbo_name).loadable());

    std::stringstream ss;

    bool first = true;

    for (auto* filter : filters_)
    {
        loginf << "FilterManager: getSQLCondition: filter " << filter->instanceId() << " active "
               << filter->getActive() << " filters " << dbo_name << " "
               << filter->filters(dbo_name);

        if (filter->getActive() && filter->filters(dbo_name))
        {
            ss << filter->getConditionString(dbo_name, first, filtered_variables);
        }
    }

    logdbg << "FilterManager: getSQLCondition: name " << dbo_name << " '" << ss.str() << "'";
    return ss.str();
}

unsigned int FilterManager::getNumFilters() { return filters_.size(); }

DBFilter* FilterManager::getFilter(unsigned int index)
{
    assert(index < filters_.size());

    return filters_.at(index);
}

void FilterManager::reset()
{
    for (unsigned int cnt = 0; cnt < filters_.size(); cnt++)
    {
        if (!filters_.at(cnt)->disabled())
            filters_.at(cnt)->reset();
    }
}

void FilterManager::deleteFilterSlot(DBFilter* filter)
{
    std::vector<DBFilter*>::iterator it;

    it = find(filters_.begin(), filters_.end(), filter);
    if (it == filters_.end())
        throw std::runtime_error("FilterManager: deleteFilter: called with unknown filter");
    else
    {
        filters_.erase(it);
        delete filter;
    }

    emit changedFiltersSignal();
}

void FilterManager::unshowViewPointSlot (ViewPoint* vp)
{
    loginf << "FilterManager: unshowViewPointSlot";
    assert (vp);
}

void FilterManager::showViewPointSlot (ViewPoint* vp)
{
    loginf << "FilterManager: showViewPointSlot";
    assert (vp);

    json& data = vp->data();

    DBObjectManager& obj_man = ATSDB::instance().objectManager();

    // add all db objects that need loading
    if (data.contains("db_objects")) // the listed ones should be loaded
    {
        json& db_objects  = data.at("db_objects");
        for (auto& obj_it : obj_man)
            obj_it.second->loadingWanted(
                        std::find(db_objects.begin(), db_objects.end(), obj_it.first) != db_objects.end());
    }
    else // all should be loaded
    {
        for (auto& obj_it : obj_man)
            obj_it.second->loadingWanted(true);
    }

    // add filters
    obj_man.useFilters(data.contains("filters"));

    if (data.contains("filters"))
    {
        json& filters = data.at("filters");
        assert (filters.is_array());

        disableAllFilters();

        for (auto& fil_it : filters.get<json::array_t>())
        {
            assert (fil_it.contains("name"));
            assert (fil_it.contains("conditions"));
            const std::string& fil_name = fil_it.at("name");

            auto it = find_if(filters_.begin(), filters_.end(),
                              [fil_name] (const DBFilter* f) { return f->getName() == fil_name; } );

            assert (it != filters_.end());
            (*it)->setActive(true);
            (*it)->loadViewPointConditions(fil_it.at("conditions"));
        }
    }
}

void FilterManager::setConfigInViewPoint (ViewPoint& vp)
{
    loginf << "FilterManager: setConfigInViewPoint";

    DBObjectManager& obj_man = ATSDB::instance().objectManager();

    vp.data()["db_objects"] = json::array();
    json& db_objects = vp.data()["db_objects"];

    // add all db objects that need loading
    unsigned int cnt=0;
    for (auto& obj_it : obj_man)
    {
        if (obj_it.second->loadable() && obj_it.second->loadingWanted())
        {
           db_objects[cnt] = obj_it.first;
           ++cnt;
        }
    }

    // add filters
    if (obj_man.useFilters())
    {
        vp.data()["filters"] = json::array();
        json& filters = vp.data().at("filters");

        unsigned int cnt=0;
        for (auto fil_it : filters_)
        {
            if (fil_it->getActive())
            {
                json& filter = filters[cnt];

                filter["name"] = fil_it->getName();
                filter["conditions"] = json::array();
                json& conditions = filter.at("conditions");

                fil_it->saveViewPointConditions(conditions);

                ++cnt;
            }
        }

        loginf << "FilterManager: setConfigInViewPoint: filters: '" << filters.dump(4) << "'";
    }
}

FilterManagerWidget* FilterManager::widget()
{
    if (!widget_)
    {
        widget_ = new FilterManagerWidget(*this);
        connect(this, SIGNAL(changedFiltersSignal()), widget_, SLOT(updateFiltersSlot()));
    }

    assert(widget_);
    return widget_;
}

void FilterManager::startedSlot()
{
    loginf << "FilterManager: startedSlot";
    createSubConfigurables();

    std::string tmpstr = ATSDB::instance().interface().connection().identifier();
    replace(tmpstr.begin(), tmpstr.end(), ' ', '_');

    if (db_id_.compare(tmpstr) != 0)
    {
        loginf << "FilterManager: startedSlot: different db id, resetting filters";
        reset();
        db_id_ = tmpstr;
    }

    emit changedFiltersSignal();

    if (widget_)
        widget_->databaseOpenedSlot();
}

void FilterManager::disableAllFilters ()
{
    for (auto fil_it : filters_)
        fil_it->setActive(false);
}
