/*
 * This file is part of OpenATS COMPASS.
 *
 * COMPASS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * COMPASS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with COMPASS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "compass.h"

#include <qobject.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "config.h"
#include "dbinterface.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "dbschemamanager.h"
#include "dbtableinfo.h"
#include "filtermanager.h"
#include "global.h"
#include "jobmanager.h"
#include "logger.h"
#include "projectionmanager.h"
#include "taskmanager.h"
#include "viewmanager.h"
#include "evaluationmanager.h"

using namespace std;

/**
 * Sets init state, creates members, starts the thread using go.
 */
COMPASS::COMPASS() : Configurable("COMPASS", "COMPASS0", 0, "compass.json")
{
    logdbg << "COMPASS: constructor: start";

    simple_config_.reset(new SimpleConfig("config.json"));

    JobManager::instance().start();

    createSubConfigurables();

    assert(db_interface_);
    assert(db_schema_manager_);
    assert(dbo_manager_);
    assert(filter_manager_);
    assert(task_manager_);
    assert(view_manager_);
    assert(eval_manager_);

    QObject::connect(db_schema_manager_.get(), &DBSchemaManager::schemaChangedSignal,
                     dbo_manager_.get(), &DBObjectManager::updateSchemaInformationSlot);
    // QObject::connect (db_schema_manager_, SIGNAL(schemaLockedSignal()), dbo_manager_,
    // SLOT(schemaLockedSlot()));
    QObject::connect(db_interface_.get(), &DBInterface::databaseContentChangedSignal,
                     db_schema_manager_.get(), &DBSchemaManager::databaseContentChangedSlot,
                     Qt::QueuedConnection);
    QObject::connect(db_interface_.get(), &DBInterface::databaseContentChangedSignal,
                     dbo_manager_.get(), &DBObjectManager::databaseContentChangedSlot,
                     Qt::QueuedConnection);
    // QObject::connect(db_interface_, SIGNAL(databaseOpenedSignal()), filter_manager_,
    // SLOT(databaseOpenedSlot()));

    QObject::connect(dbo_manager_.get(), &DBObjectManager::dbObjectsChangedSignal,
                     task_manager_.get(), &TaskManager::dbObjectsChangedSlot);
    QObject::connect(dbo_manager_.get(), &DBObjectManager::schemaChangedSignal, task_manager_.get(),
                     &TaskManager::schemaChangedSlot);

    dbo_manager_->updateSchemaInformationSlot();

    logdbg << "COMPASS: constructor: end";
}

// void COMPASS::initialize()
//{
//    assert (!initialized_);
//    initialized_=true;

//}

/**
 * Deletes members.
 */
COMPASS::~COMPASS()
{
    logdbg << "COMPASS: destructor: start";

    if (!shut_down_)
    {
        logerr << "COMPASS: destructor: not shut down";
        shutdown();
    }

    // assert (!initialized_);

    assert(!dbo_manager_);
    assert(!db_schema_manager_);
    assert(!db_interface_);
    assert(!filter_manager_);
    assert(!task_manager_);
    assert(!view_manager_);
    assert (!eval_manager_);

    logdbg << "COMPASS: destructor: end";
}

void COMPASS::generateSubConfigurable(const std::string& class_id, const std::string& instance_id)
{
    logdbg << "COMPASS: generateSubConfigurable: class_id " << class_id << " instance_id "
           << instance_id;
    if (class_id == "DBInterface")
    {
        assert(!db_interface_);
        db_interface_.reset(new DBInterface(class_id, instance_id, this));
        assert(db_interface_);
    }
    else if (class_id == "DBObjectManager")
    {
        assert(!dbo_manager_);
        dbo_manager_.reset(new DBObjectManager(class_id, instance_id, this));
        assert(dbo_manager_);
    }
    else if (class_id == "DBSchemaManager")
    {
        assert(db_interface_);
        assert(!db_schema_manager_);
        db_schema_manager_.reset(new DBSchemaManager(class_id, instance_id, this, *db_interface_));
        assert(db_schema_manager_);
    }
    else if (class_id == "FilterManager")
    {
        assert(!filter_manager_);
        filter_manager_.reset(new FilterManager(class_id, instance_id, this));
        assert(filter_manager_);
    }
    else if (class_id == "TaskManager")
    {
        assert(!task_manager_);
        task_manager_.reset(new TaskManager(class_id, instance_id, this));
        assert(task_manager_);
    }
    else if (class_id == "ViewManager")
    {
        assert(!view_manager_);
        view_manager_.reset(new ViewManager(class_id, instance_id, this));
        assert(view_manager_);
    }
    else if (class_id == "EvaluationManager")
    {
        assert(!eval_manager_);
        eval_manager_.reset(new EvaluationManager(class_id, instance_id, this));
        assert(eval_manager_);
    }
    else
        throw std::runtime_error("COMPASS: generateSubConfigurable: unknown class_id " + class_id);
}

void COMPASS::checkSubConfigurables()
{
    if (!db_interface_)
    {
        addNewSubConfiguration("DBInterface", "DBInterface0");
        generateSubConfigurable("DBInterface", "DBInterface0");
        assert(db_interface_);
    }
    if (!dbo_manager_)
    {
        assert(db_interface_);
        addNewSubConfiguration("DBObjectManager", "DBObjectManager0");
        generateSubConfigurable("DBObjectManager", "DBObjectManager0");
        assert(dbo_manager_);
    }
    if (!db_schema_manager_)
    {
        addNewSubConfiguration("DBSchemaManager", "DBSchemaManager0");
        generateSubConfigurable("DBSchemaManager", "DBSchemaManager0");
        assert(dbo_manager_);
    }
    if (!filter_manager_)
    {
        addNewSubConfiguration("FilterManager", "FilterManager0");
        generateSubConfigurable("FilterManager", "FilterManager0");
        assert(filter_manager_);
    }
    if (!task_manager_)
    {
        addNewSubConfiguration("TaskManager", "TaskManager0");
        generateSubConfigurable("TaskManager", "TaskManager0");
        assert(task_manager_);
    }
    if (!view_manager_)
    {
        addNewSubConfiguration("ViewManager", "ViewManager0");
        generateSubConfigurable("ViewManager", "ViewManager0");
        assert(view_manager_);
    }
    if (!eval_manager_)
    {
        addNewSubConfiguration("EvaluationManager", "EvaluationManager0");
        generateSubConfigurable("EvaluationManager", "EvaluationManager0");
        assert(eval_manager_);
    }
}

DBInterface& COMPASS::interface()
{
    assert(db_interface_);
    // assert (initialized_);
    return *db_interface_;
}

DBSchemaManager& COMPASS::schemaManager()
{
    assert(db_schema_manager_);
    // assert (initialized_);
    return *db_schema_manager_;
}

DBObjectManager& COMPASS::objectManager()
{
    assert(dbo_manager_);
    // assert (initialized_);
    return *dbo_manager_;
}

FilterManager& COMPASS::filterManager()
{
    assert(filter_manager_);
    // assert (initialized_);
    return *filter_manager_;
}

TaskManager& COMPASS::taskManager()
{
    assert(task_manager_);
    // assert (initialized_);
    return *task_manager_;
}

ViewManager& COMPASS::viewManager()
{
    assert(view_manager_);
    // assert (initialized_);
    return *view_manager_;
}

SimpleConfig& COMPASS::config()
{
    // assert (initialized_);
    assert(simple_config_);
    return *simple_config_;
}

EvaluationManager& COMPASS::evaluationManager()
{
    assert(eval_manager_);
    return *eval_manager_;
}

bool COMPASS::ready()
{
    if (!db_interface_)  // || !initialized_)
        return false;

    return db_interface_->ready();
}

///**
// * Calls stop. If data was written uning the StructureReader, this process is finished correctly.
// * State is set to DB_STATE_SHUTDOWN and ouput buffers are cleared.
// */
void COMPASS::shutdown()
{
    loginf << "COMPASS: database shutdown";

    if (shut_down_)
    {
        logerr << "COMPASS: already shut down";
        return;
    }

    JobManager::instance().shutdown();
    ProjectionManager::instance().shutdown();

    assert(eval_manager_);
    eval_manager_->close();
    eval_manager_ = nullptr;

    assert(view_manager_);
    view_manager_->close();
    view_manager_ = nullptr;

    assert(db_interface_);
    db_interface_->closeConnection();  // removes connection widgets, needs to be before

    assert(dbo_manager_);
    dbo_manager_ = nullptr;

    assert(db_schema_manager_);
    db_schema_manager_ = nullptr;

    assert(task_manager_);
    task_manager_->shutdown();
    task_manager_ = nullptr;

    assert(db_interface_);
    db_interface_ = nullptr;

    assert(filter_manager_);
    filter_manager_ = nullptr;

    shut_down_ = true;

    loginf << "COMPASS: shutdown: end";
}
