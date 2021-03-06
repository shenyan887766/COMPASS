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

#ifndef EVALUATIONREQUIREMENTIDENTIFICATIONCONFIG_H
#define EVALUATIONREQUIREMENTIDENTIFICATIONCONFIG_H

#include "configurable.h"
#include "eval/requirement/config.h"
#include "eval/requirement/identification/identification.h"
#include "eval/requirement/identification/identificationconfigwidget.h"

#include <memory>


class Group;
class EvaluationStandard;

namespace EvaluationRequirement
{

    class IdentificationConfig : public Config
    {
    public:
        IdentificationConfig(const std::string& class_id, const std::string& instance_id,
                        Group& group, EvaluationStandard& standard,
                        EvaluationManager& eval_man);
        virtual ~IdentificationConfig();

        virtual void addGUIElements(QFormLayout* layout) override;
        IdentificationConfigWidget* widget() override;
        std::shared_ptr<Base> createRequirement() override;

        float maxRefTimeDiff() const;
        void maxRefTimeDiff(float value);

        float minimumProbability() const;
        void minimumProbability(float value);

    protected:
        float max_ref_time_diff_ {0};
        float minimum_probability_{0};

        std::unique_ptr<IdentificationConfigWidget> widget_;
    };

}

#endif // EVALUATIONREQUIREMENTIDENTIFICATIONCONFIG_H
