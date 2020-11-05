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

#include "createassociationsjob.h"
#include "compass.h"
#include "buffer.h"
#include "createassociationstask.h"
#include "dbinterface.h"
#include "dbobject.h"
#include "dbobjectmanager.h"
#include "metadbovariable.h"
#include "dbovariable.h"

using namespace std;
using namespace Utils;

CreateAssociationsJob::CreateAssociationsJob(CreateAssociationsTask& task, DBInterface& db_interface,
                                             std::map<std::string, std::shared_ptr<Buffer>> buffers)
    : Job("CreateAssociationsJob"), task_(task), db_interface_(db_interface), buffers_(buffers)
{
}

CreateAssociationsJob::~CreateAssociationsJob() {}

void CreateAssociationsJob::run()
{
    logdbg << "CreateAssociationsJob: run: start";

    started_ = true;

    boost::posix_time::ptime start_time;
    boost::posix_time::ptime stop_time;

    start_time = boost::posix_time::microsec_clock::local_time();

    loginf << "CreateARTASAssociationsJob: run: clearing associations";

    DBObjectManager& object_man = COMPASS::instance().objectManager();

    object_man.removeAssociations();

    // create target reports
    emit statusSignal("Creating Target Reports");
    createTargetReports();

    // create utns
    emit statusSignal("Creating UTNs");
    createUTNS();

    // save associations
    emit statusSignal("Saving Associations");
    for (auto& dbo_it : object_man)
    {
        loginf << "CreateARTASAssociationsJob: run: processing object " << dbo_it.first
               << " associated " << dbo_it.second->associations().size() << " of "
               << dbo_it.second->count();
        dbo_it.second->saveAssociations();
    }

    object_man.setAssociationsByAll(); // no specific dbo or data source

    stop_time = boost::posix_time::microsec_clock::local_time();

    double load_time;
    boost::posix_time::time_duration diff = stop_time - start_time;
    load_time = diff.total_milliseconds() / 1000.0;

    loginf << "CreateARTASAssociationsJob: run: done ("
           << String::doubleToStringPrecision(load_time, 2) << " s).";
    done_ = true;
}

void CreateAssociationsJob::createTargetReports()
{
    loginf << "CreateAssociationsJob: createTargetReports";

    MetaDBOVariable* meta_key_var = task_.keyVar();
    MetaDBOVariable* meta_ds_id_var = task_.dsIdVar();
    MetaDBOVariable* meta_tod_var = task_.todVar();
    MetaDBOVariable* meta_ta_var = task_.targetAddrVar();
    MetaDBOVariable* meta_ti_var = task_.targetIdVar();
    MetaDBOVariable* meta_tn_var = task_.trackNumVar();
    MetaDBOVariable* meta_mode_3a_var = task_.mode3AVar();
    MetaDBOVariable* meta_mode_c_var = task_.modeCVar();
    MetaDBOVariable* meta_latitude_var = task_.latitudeVar();
    MetaDBOVariable* meta_longitude_var = task_.longitudeVar();

    assert (meta_key_var);
    assert (meta_ds_id_var);
    assert (meta_tod_var);
    assert (meta_ta_var);
    assert (meta_ti_var);
    assert (meta_tn_var);
    assert (meta_mode_3a_var);
    assert (meta_mode_c_var);
    assert (meta_latitude_var);
    assert (meta_longitude_var);

    DBObjectManager& object_man = COMPASS::instance().objectManager();

    for (auto& buf_it : buffers_) // dbo name, buffer
    {
        string dbo_name = buf_it.first;
        DBObject& dbo = object_man.object(dbo_name);

        shared_ptr<Buffer> buffer = buf_it.second;
        size_t buffer_size = buffer->size();

        assert (meta_key_var->existsIn(dbo_name));
        DBOVariable& key_var = meta_key_var->getFor(dbo_name);

        assert (meta_ds_id_var->existsIn(dbo_name));
        DBOVariable& ds_id_var = meta_ds_id_var->getFor(dbo_name);

        assert (meta_tod_var->existsIn(dbo_name));
        DBOVariable& tod_var = meta_tod_var->getFor(dbo_name);

        assert (meta_ta_var->existsIn(dbo_name));
        DBOVariable& ta_var = meta_ta_var->getFor(dbo_name);

        assert (meta_ti_var->existsIn(dbo_name));
        DBOVariable& ti_var = meta_ti_var->getFor(dbo_name);

        DBOVariable* tn_var {nullptr}; // not in ads-b
        if (meta_tn_var->existsIn(dbo_name))
                tn_var = &meta_tn_var->getFor(dbo_name);

        assert (meta_mode_3a_var->existsIn(dbo_name));
        DBOVariable& mode_3a_var = meta_mode_3a_var->getFor(dbo_name);

        assert (meta_mode_c_var->existsIn(dbo_name));
        DBOVariable& mode_c_var = meta_mode_c_var->getFor(dbo_name);

        assert (meta_latitude_var->existsIn(dbo_name));
        DBOVariable& latitude_var = meta_latitude_var->getFor(dbo_name);

        assert (meta_longitude_var->existsIn(dbo_name));
        DBOVariable& longitude_var = meta_longitude_var->getFor(dbo_name);


        assert (buffer->has<int>(key_var.name()));
        NullableVector<int>& rec_nums = buffer->get<int>(key_var.name());

        assert (buffer->has<int>(ds_id_var.name()));
        NullableVector<int>& ds_ids = buffer->get<int>(ds_id_var.name());

        assert (buffer->has<float>(tod_var.name()));
        NullableVector<float>& tods = buffer->get<float>(tod_var.name());

        assert (buffer->has<int>(ta_var.name()));
        NullableVector<int>& tas = buffer->get<int>(ta_var.name());

        assert (buffer->has<string>(ti_var.name()));
        NullableVector<string>& tis = buffer->get<string>(ti_var.name());

        NullableVector<int>* tns {nullptr};
        if (tn_var)
        {
            assert (buffer->has<int>(tn_var->name()));
            tns = &buffer->get<int>(tn_var->name());
        }

        assert (buffer->has<int>(mode_3a_var.name()));
        NullableVector<int>& m3as = buffer->get<int>(mode_3a_var.name());

        assert (buffer->has<int>(mode_c_var.name()));
        NullableVector<int>& mcs = buffer->get<int>(mode_c_var.name());

        assert (buffer->has<double>(latitude_var.name()));
        NullableVector<double>& lats = buffer->get<double>(latitude_var.name());

        assert (buffer->has<double>(longitude_var.name()));
        NullableVector<double>& longs = buffer->get<double>(longitude_var.name());

        for (size_t cnt = 0; cnt < buffer_size; ++cnt)
        {
            assert (!rec_nums.isNull(cnt));
        }
    }
}

void CreateAssociationsJob::createUTNS()
{
    loginf << "CreateAssociationsJob: createUTNS";

    MetaDBOVariable* meta_key_var = task_.keyVar();
    MetaDBOVariable* meta_tod_var = task_.todVar();
    MetaDBOVariable* meta_ta_var = task_.targetAddrVar();

    assert (meta_key_var);
    assert (meta_tod_var);
    assert (meta_ta_var);

    DBObjectManager& object_man = COMPASS::instance().objectManager();

    for (auto& buf_it : buffers_) // dbo name, buffer
    {
        string dbo_name = buf_it.first;
        DBObject& dbo = object_man.object(dbo_name);

        shared_ptr<Buffer> buffer = buf_it.second;
        size_t buffer_size = buffer->size();

        assert (meta_key_var->existsIn(dbo_name));
        DBOVariable& key_var = meta_key_var->getFor(dbo_name);

        assert (meta_tod_var->existsIn(dbo_name));
        DBOVariable& tod_var = meta_tod_var->getFor(dbo_name);

        assert (meta_ta_var->existsIn(dbo_name));
        DBOVariable& ta_var = meta_ta_var->getFor(dbo_name);

        assert (buffer->has<int>(key_var.name()));
        assert (buffer->has<float>(tod_var.name()));
        assert (buffer->has<int>(ta_var.name()));

        NullableVector<int>& rec_nums = buffer->get<int>(key_var.name());
        NullableVector<float>& tods = buffer->get<float>(tod_var.name());
        NullableVector<int>& tas = buffer->get<int>(ta_var.name());

        unsigned int rec_num;
        unsigned int target_addr;
        unsigned int utn;

        for (size_t cnt = 0; cnt < buffer_size; ++cnt)
        {
            assert (!rec_nums.isNull(cnt));

            if (tas.isNull(cnt))
                continue;

            rec_num = rec_nums.get(cnt);
            target_addr = tas.get(cnt);

            if (ta_2_utn_.count(target_addr) == 0)
            {
                ta_2_utn_[target_addr] = utn_cnt_;
                ++utn_cnt_;
            }

            utn = ta_2_utn_[target_addr];

            dbo.addAssociation(rec_num, utn, false, 0);
        }

        dbo.saveAssociations();
    }

}
