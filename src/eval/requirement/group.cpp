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

#include "eval/requirement/group.h"
#include "evaluationstandard.h"
#include "eval/requirement/detection/detectionconfig.h"
#include "eval/requirement/position/positionmaxdistanceconfig.h"
#include "eval/requirement/identification/identificationconfig.h"
#include "logger.h"

#include <QInputDialog>
#include <QMessageBox>

#include <algorithm>

using namespace std;

Group::Group(const std::string& class_id, const std::string& instance_id,
                                                       EvaluationStandard& standard, EvaluationManager& eval_man)
    : Configurable(class_id, instance_id, &standard), EvaluationStandardTreeItem(&standard), standard_(standard),
      eval_man_(eval_man)
{
    registerParameter("name", &name_, "");

    assert (name_.size());

    createSubConfigurables();
}

Group::~Group()
{

}


void Group::generateSubConfigurable(const std::string& class_id,
                                                         const std::string& instance_id)
{
    if (class_id.compare("EvaluationRequirementDetectionConfig") == 0)
    {
        EvaluationRequirement::DetectionConfig* config =
                new EvaluationRequirement::DetectionConfig(
                    class_id, instance_id, *this, standard_, eval_man_);
        logdbg << "EvaluationRequirementGroup: generateSubConfigurable: adding config " << config->name();

        assert(!hasRequirementConfig(config->name()));
        configs_.push_back(std::unique_ptr<EvaluationRequirement::Config>(config));
    }
    else if (class_id.compare("EvaluationRequirementPositionMaxDistanceConfig") == 0)
    {
        EvaluationRequirement::PositionMaxDistanceConfig* config =
                new EvaluationRequirement::PositionMaxDistanceConfig(
                    class_id, instance_id, *this, standard_, eval_man_);
        logdbg << "EvaluationRequirementGroup: generateSubConfigurable: adding config " << config->name();

        assert(!hasRequirementConfig(config->name()));
        configs_.push_back(std::unique_ptr<EvaluationRequirement::Config>(config));
    }
    else if (class_id.compare("EvaluationRequirementIdentificationConfig") == 0)
    {
        EvaluationRequirement::IdentificationConfig* config =
                new EvaluationRequirement::IdentificationConfig(
                    class_id, instance_id, *this, standard_, eval_man_);
        logdbg << "EvaluationRequirementGroup: generateSubConfigurable: adding config " << config->name();

        assert(!hasRequirementConfig(config->name()));
        configs_.push_back(std::unique_ptr<EvaluationRequirement::Config>(config));
    }
    else
        throw std::runtime_error("EvaluationStandard: generateSubConfigurable: unknown class_id " +
                                 class_id);
}

std::string Group::name() const
{
    return name_;
}


bool Group::hasRequirementConfig (const std::string& name)
{
    auto iter = std::find_if(configs_.begin(), configs_.end(),
       [&name](const unique_ptr<EvaluationRequirement::Config>& x) { return x->name() == name;});

    return iter != configs_.end();
}

void Group::addRequirementConfig (const std::string& class_id, const std::string& name,
                                                       const std::string& short_name)
{
    loginf << "EvaluationRequirementGroup: addRequirementConfig: class_id " << class_id << " name " << name
           << " short_name " << short_name;

    assert (!hasRequirementConfig(name));

    standard_.beginModelReset();

    std::string instance = class_id + name + "0";

    Configuration& config = addNewSubConfiguration(class_id, instance);
    config.addParameterString("name", name);
    config.addParameterString("short_name", short_name);

    generateSubConfigurable(class_id, instance);

    sortConfigs();

    standard_.endModelReset();

    assert (hasRequirementConfig(name));

    emit configsChangedSignal();
}

EvaluationRequirement::Config& Group::requirementConfig (const std::string& name)
{
    assert (hasRequirementConfig(name));

    auto iter = std::find_if(configs_.begin(), configs_.end(),
                             [&name](const unique_ptr<EvaluationRequirement::Config>& x) { return x->name() == name;});

    return **iter;
}

void Group::removeRequirementConfig (const std::string& name)
{
    assert (hasRequirementConfig(name));

    auto iter = std::find_if(configs_.begin(), configs_.end(),
                             [&name](const unique_ptr<EvaluationRequirement::Config>& x) { return x->name() == name;});

    configs_.erase(iter);

    emit configsChangedSignal();
}

void Group::checkSubConfigurables()
{

}

EvaluationStandardTreeItem* Group::child(int row)
{
    if (row < 0 || row >= configs_.size())
        return nullptr;

    return configs_.at(row).get();
}

int Group::childCount() const
{
    return configs_.size();
}

int Group::columnCount() const
{
    return 1;
}

QVariant Group::data(int column) const
{
    assert (column == 0);

    return name_.c_str();
}

int Group::row() const
{
    return 0;
}

void Group::showMenu ()
{
    QMenu menu;

    // menu creation
    {
        QAction* del_action = menu.addAction("Delete Group");
        connect(del_action, &QAction::triggered, this, &Group::deleteGroupSlot);

        // requirements
        QMenu* req_menu = menu.addMenu("Add Requirement");;

        { // detection
            QAction* add_det_action = req_menu->addAction("Detection");
            add_det_action->setData("EvaluationRequirementDetectionConfig");
            connect(add_det_action, &QAction::triggered, this, &Group::addRequirementSlot);
        }

        { // identification
            QAction* add_pos_action = req_menu->addAction("Identification");
            add_pos_action->setData("EvaluationRequirementIdentificationConfig");
            connect(add_pos_action, &QAction::triggered, this, &Group::addRequirementSlot);
        }

        { // position
            QAction* add_pos_action = req_menu->addAction("Position");
            add_pos_action->setData("EvaluationRequirementPositionMaxDistanceConfig");
            connect(add_pos_action, &QAction::triggered, this, &Group::addRequirementSlot);
        }

    }

    menu.exec(QCursor::pos());
}

void Group::deleteGroupSlot()
{
    loginf << "EvaluationRequirementGroup " << name_ << ": deleteGroupSlot";

    standard_.removeGroup (name_);
}

void Group::addRequirementSlot()
{
    loginf << "EvaluationRequirementGroup " << name_ << ": addRequirementSlot";

    QAction* action = dynamic_cast<QAction*>(QObject::sender());
    assert (action);

    QVariant data = action->data();
    assert (data.isValid());

    string class_id = data.toString().toStdString();

    loginf << "EvaluationRequirementGroup " << name_ << ": addRequirementSlot: class_id " << class_id;

    bool ok;
    QString text =
            QInputDialog::getText(nullptr, tr("Requirement Name"),
                                  tr("Specify a (unique) requirement name:"), QLineEdit::Normal,
                                  "", &ok);

    std::string req_name;

    if (ok && !text.isEmpty())
    {
        req_name = text.toStdString();
        if (!req_name.size())
        {
            QMessageBox m_warning(QMessageBox::Warning, "Adding Requirement Failed",
                                  "Requirement has to have a non-empty name.", QMessageBox::Ok);
            m_warning.exec();
            return;
        }

        if (hasRequirementConfig(req_name))
        {
            QMessageBox m_warning(QMessageBox::Warning, "Adding Requirement Failed",
                                  "Requirement with this name already exists.", QMessageBox::Ok);
            m_warning.exec();
            return;
        }
    }

    std::string req_short_name;

    text =  QInputDialog::getText(nullptr, tr("Requirement Short Name"),
                                  tr("Specify a requirement short name:"), QLineEdit::Normal,
                                  "", &ok);

    if (ok && !text.isEmpty())
    {
        req_short_name = text.toStdString();
    }

    loginf << "EvaluationRequirementGroup " << name_ << ": addRequirementSlot: class_id " << class_id
           << " req_name '" << req_name << "' req_short_name '" << req_short_name << "'";

    addRequirementConfig(class_id, req_name, req_short_name);
}

void Group::sortConfigs()
{
    sort(configs_.begin(), configs_.end(),
         [](const unique_ptr<EvaluationRequirement::Config>&a, const unique_ptr<EvaluationRequirement::Config>& b) -> bool
    {
        return a->name() > b->name();
    });
}
