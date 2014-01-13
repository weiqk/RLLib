/*
 * MountainCarModel.cpp
 *
 *  Created on: Oct 17, 2013
 *      Author: sam
 */

#include "MountainCarModel.h"

using namespace RLLibViz;

MountainCarModel::MountainCarModel(QObject *parent) :
    ModelBase(parent)
{
  // RLLib:
  Probabilistic::srand(0);
  behaviourEnvironment = new MountainCar<double>;
  evaluationEnvironment = new MountainCar<double>;
  hashing = new MurmurHashing(1000000);
  projector = new TileCoderHashing<double>(hashing, behaviourEnvironment->dimension(), 10, 10,
      true);
  toStateAction = new StateActionTilings<double>(projector,
      behaviourEnvironment->getDiscreteActions());

  alpha_v = 0.05 / projector->vectorNorm();
  alpha_w = 0.0001 / projector->vectorNorm();
  gamma = 0.99;
  lambda = 0.0; //0.4;
  critice = new ATrace<double>(projector->dimension());
  critic = new GTDLambda<double>(alpha_v, alpha_w, gamma, lambda, critice);

  alpha_u = 1.0 / projector->vectorNorm();

  target = new BoltzmannDistribution<double>(projector->dimension(),
      behaviourEnvironment->getDiscreteActions());

  actore = new ATrace<double>(projector->dimension());
  actoreTraces = new Traces<double>();
  actoreTraces->push_back(actore);
  actor = new ActorLambdaOffPolicy<double>(alpha_u, gamma, lambda, target, actoreTraces);

  behavior = new RandomPolicy<double>(behaviourEnvironment->getDiscreteActions());
  control = new OffPAC<double>(behavior, critic, actor, toStateAction, projector);

  learningAgent = new LearnerAgent<double>(control);
  evaluationAgent = new ControlAgent<double>(control);

  learningRunner = new Simulator<double>(learningAgent, behaviourEnvironment, 5000);
  evaluationRunner = new Simulator<double>(evaluationAgent, evaluationEnvironment, 5000);
  learningRunner->setVerbose(false);
  evaluationRunner->setVerbose(false);

  simulators.insert(std::make_pair(simulators.size(), learningRunner));
  simulators.insert(std::make_pair(simulators.size(), evaluationRunner));

  valueFunction = new Matrix(100, 100);

}

MountainCarModel::~MountainCarModel()
{
  delete behaviourEnvironment;
  delete evaluationEnvironment;
  delete hashing;
  delete projector;
  delete toStateAction;
  delete critice;
  delete critic;
  delete actore;
  delete actoreTraces;
  delete actor;
  delete behavior;
  delete target;
  delete control;
  delete learningAgent;
  delete evaluationAgent;
  delete learningRunner;
  delete evaluationRunner;
  delete valueFunction;
}

void MountainCarModel::initialize()
{
  ModelBase::initialize();
}

void MountainCarModel::doWork()
{
  for (std::tr1::unordered_map<int, Simulator<double>*>::iterator i = simulators.begin();
      i != simulators.end(); ++i)
    i->second->step();

  for (std::tr1::unordered_map<int, Simulator<double>*>::iterator i = simulators.begin();
      i != simulators.end(); ++i)
  {
    if (i->second->isEndingOfEpisode())
    {
      emit signal_draw(window->views[i->first]);
      emit signal_add(window->plots[i->first], Vec(i->second->timeStep, 0),
          Vec(i->second->episodeR, 0));
      emit signal_draw(window->plots[i->first]);
    }
    else
      emit signal_add(window->views[i->first],
          Vec(i->second->getRLProblem()->getObservations()->at(0),
              i->second->getRLProblem()->getObservations()->at(1)), Vec(0.0, 0.0, 0.0, 1.0));
  }

  // Value function
  if (evaluationRunner->isEndingOfEpisode() && window->vfuns.size() > 1)
  {
    RLLib::PVector<double> x_t(2);
    double maxValue = 0, minValue = 0;
    const Range<double>* positionRange = behaviourEnvironment->getObservationRanges()->at(0);
    const Range<double>* velocityRange = behaviourEnvironment->getObservationRanges()->at(1);

    for (int position = 0; position < valueFunction->rows(); position++)
    {
      for (int velocity = 0; velocity < valueFunction->cols(); velocity++)
      {
        x_t[0] = positionRange->toUnit(
            positionRange->length() * position / valueFunction->cols() + positionRange->min());
        x_t[1] = velocityRange->toUnit(
            velocityRange->length() * velocity / valueFunction->rows() + velocityRange->min());
        double v = control->computeValueFunction(&x_t);
        valueFunction->at(position, velocity) = v;
        if (v > maxValue)
          maxValue = v;
        if (v < minValue)
          minValue = v;
      }
    }
    //out.close();
    emit signal_add(window->vfuns[1], valueFunction, minValue, maxValue);
    emit signal_draw(window->vfuns[1]);
  }
}

