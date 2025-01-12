/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  num_particles = 10;  // TODO: Set the number of particles

  std::default_random_engine gen;

  std::normal_distribution<double> dist_x(x, std[0]);
  std::normal_distribution<double> dist_y(y, std[1]);
  std::normal_distribution<double> dist_theta(theta, std[2]);

  for (int i = 0; i < num_particles; ++i) {
    Particle p;
    p.id = i;
    p.x = dist_x(gen);
    p.y = dist_y(gen);
    p.theta = dist_theta(gen);
    p.weight = 1.0;
    particles.push_back(p);
  }

  is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  std::default_random_engine gen;

  for (int i = 0; i < num_particles; ++i) {
    double x0 = particles[i].x;
    double y0 = particles[i].y;
    double theta0 = particles[i].theta;
    double xf, yf, thetaf;

    if (fabs(yaw_rate) < 0.0001) {
      xf = x0 + velocity * cos(theta0) * delta_t;
      yf = y0 + velocity * sin(theta0) * delta_t;
      thetaf = theta0;
    }
    else {
      xf = x0 + (velocity / yaw_rate) * (sin(theta0 + yaw_rate * delta_t) - sin(theta0));
      yf = y0 + (velocity / yaw_rate) * (cos(theta0) - cos(theta0 + yaw_rate * delta_t));
      thetaf = theta0 + yaw_rate * delta_t;
    }

    std::normal_distribution<double> dist_xf(xf, std_pos[0]);
    std::normal_distribution<double> dist_yf(yf, std_pos[1]);
    std::normal_distribution<double> dist_thetaf(thetaf, std_pos[2]);

    particles[i].x = dist_xf(gen);
    particles[i].y = dist_yf(gen);
    particles[i].theta = dist_thetaf(gen);
  }

}

// Unused function
void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  for (uint i = 0; i < observations.size(); ++i) {
    LandmarkObs obs = observations[i];
    int id = -1;
    double min_dist = std::numeric_limits<double>::max();
    for (LandmarkObs pre : predicted) {
      double d = dist(obs.x, obs.y, pre.x, pre.y);
      if (d < min_dist) {
        id = pre.id;
        min_dist = d;
      }
    }
    observations[i].id = id;
  }

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */

  // Reset weights
  weights.clear();

  for (int i = 0; i < num_particles; ++i) {
    double prob = 1.0;
    for (LandmarkObs obs : observations) {
      // 1. Transform the observation to the coordinates of the map
      LandmarkObs transformed_obs = TransformCoords(particles[i], obs);
      // 2. Find the nearest landmark to the (transformed) observation
      LandmarkObs best_association = LandmarkAssociation(transformed_obs, map_landmarks);
      // 3. Calculate the error between observation and the landmark
      double error = CalculateWeight(transformed_obs, best_association, std_landmark);
      // 4. Accumulate the error for each particle
      prob *= error;
    }
    particles[i].weight = prob;
    weights.push_back(prob);
  }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
  vector<Particle> new_particles;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::discrete_distribution<int> dd(weights.begin(), weights.end());

  for (int i = 0; i < num_particles; ++i) {
    new_particles.push_back(particles[dd(gen)]);
  }

  particles = new_particles;
}

LandmarkObs ParticleFilter::TransformCoords(Particle par, LandmarkObs obs) {
  LandmarkObs transformed_obs = {
    obs.id,
    obs.x * cos(par.theta) - obs.y * sin(par.theta) + par.x,
    obs.x * sin(par.theta) + obs.y * cos(par.theta) + par.y
  };
  return transformed_obs;
}

LandmarkObs ParticleFilter::LandmarkAssociation(LandmarkObs obs, Map map_landmarks) {
  double min_dist = std::numeric_limits<double>::max();
  LandmarkObs best_association;
  for (Map::single_landmark_s landmark : map_landmarks.landmark_list) {
    double d = dist(obs.x, obs.y, landmark.x_f, landmark.y_f);
    if (d < min_dist) {
      best_association.id = landmark.id_i;
      best_association.x = landmark.x_f;
      best_association.y = landmark.y_f;
      min_dist = d;
    }
  }
  return best_association;
}

double ParticleFilter::CalculateWeight(LandmarkObs obs, LandmarkObs best, double std_landmark[]) {
  double sig_x = std_landmark[0];
  double sig_y = std_landmark[1];
  double x_obs = obs.x;
  double y_obs = obs.y;
  double mu_x = best.x;
  double mu_y = best.y;

  // calculate normalization term
  double gauss_norm;
  gauss_norm = 1 / (2 * M_PI * sig_x * sig_y);

  // calculate exponent
  double exponent;
  exponent = (pow(x_obs - mu_x, 2) / (2 * pow(sig_x, 2)))
               + (pow(y_obs - mu_y, 2) / (2 * pow(sig_y, 2)));
    
  // calculate weight using normalization terms and exponent
  double weight;
  weight = gauss_norm * exp(-exponent);

  return weight;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}