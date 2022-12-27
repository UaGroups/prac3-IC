#include <vector>
#include <algorithm>
#include "genetic.h"
#include "individual.h"
#include "mlp/mlp.h"
#include "utils/utils.h"
#include <thread>
#include <random>
#include <fstream>
#include <iostream>
#include <functional>
#include <omp.h>
#include <mpich/mpi.h>

Genetic::Genetic(int population, const std::string &fileName, std::function<Individual*()> createRandomIndividual, Simulation* simulation)
{
    this->population = population;
    this->createRandomIndividual = createRandomIndividual;
    this->simulation = simulation;
    this->fileName = fileName;
    this->simulationStartTime = 0.0;
    this->generation = 1;
}

Genetic::~Genetic()
{
    for(int i = 0; i < individuals.size();i++)
    {
        delete individuals[i];
    }
    
    individuals.clear();
    
    if(simulation != NULL)
    {
        delete simulation;
        simulation = NULL;
    }
    
    std::cout<<"Genetic destroyed"<<std::endl;
}

void Genetic::initialize()
{
    if(!load())
    {
        std::cout<<"Initializing..."<<std::endl;
        individuals.clear();
        individuals.resize(population);
        
        for(int i =0;i<population;i++)
        { 
            individuals[i] = createRandomIndividual();
        }
    }
    this->simulationStartTime = omp_get_wtime();
    simulation->init(individuals);
}

bool Genetic::load()
{
    if(fileName.empty())
    {
        std::cout<<"No generation file specified"<<std::endl;
        
        return false;
    }
    
    std::cout<<"Loading generation file "<<fileName<<std::endl;
        
    std::ifstream ifs(fileName.c_str(), std::ios::binary);
    if(ifs.is_open())
    {
        std::cout<<"Reading previous state..."<<std::endl;
        ifs.read(reinterpret_cast<char*>(&generation), sizeof(generation));
        std::cout<<"Generation "<<generation<<std::endl;
        ifs.read(reinterpret_cast<char*>(&population), sizeof(population));
        std::cout<<"Individuals "<<population<<std::endl;
        individuals.clear();
        individuals.resize(population);
        for(int i =0;i<individuals.size();i++)
        {
            individuals[i] = createRandomIndividual();
            Mlp* mlp = individuals[i]->mlp;
            int numWeights = mlp->getNumWeights();
            std::vector<double> weights(numWeights);
            
            ifs.read(reinterpret_cast<char*>(&weights[0]), sizeof(double)*weights.size());
            mlp->setWeights(weights);
            
            std::vector<bool> connections(numWeights);
            std::vector<char> temp(numWeights);
            ifs.read(reinterpret_cast<char*>(&temp[0]), sizeof(char)*connections.size());
            for(int j=0;j<numWeights;j++)
            {
                connections[j] = temp[j] > 0;
            }
            
            mlp->setConnections(connections);
        }
        ifs.close();
        
        return true;
    }
    
    std::cout<<"Could not open generation file "<<fileName<<std::endl;
    
    return false;
}

void Genetic::save()
{
    if(fileName.empty()) 
        return;
        
    std::ofstream of(fileName.c_str(), std::ios::binary);
    if(of.is_open())
    {
        int individualsSize = individuals.size();
        of.write(reinterpret_cast<char*>(&generation), sizeof generation);
        of.write(reinterpret_cast<char*>(&individualsSize), sizeof individualsSize);
        for(int i = 0;i<individuals.size();i++)
        {
            std::vector<double> weights = individuals[i]->mlp->getWeights();
            of.write(reinterpret_cast<char*>(&weights[0]), sizeof(double)*weights.size());
            std::vector<bool> connections = individuals[i]->mlp->getConnections();
            //of.write(reinterpret_cast<char*>(&connections), sizeof(bool)*connections.size());
            std::copy(connections.begin(), connections.end(), std::ostreambuf_iterator<char>(of));
        }
        of.close();
    }
}

void Genetic::updateAndEvolve()
{
    // Divide el trabajo entre los procesos de acuerdo al tamaño de la población
    int numProcesses, processId;
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
    
    int chunkSize = population / numProcesses;
    int startIndex = processId * chunkSize;
    int endIndex = startIndex + chunkSize - 1;
    if (processId == numProcesses - 1)  // Asigna el resto a los últimos procesos
        endIndex = population - 1;

    for (int i = startIndex; i <= endIndex; i++)
    {
        // Calcula el fitness de cada individuo en el rango asignado al proceso actual
        individuals[i]->calculateFitness();
    }

    // Sincroniza los resultados entre los procesos
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Si este es el proceso principal (con identificador 0), procede a evolucionar la población
    if (processId == 0) 
    {
        // Ordena la población de acuerdo a su fitness
        std::sort(individuals.begin(), individuals.end(), [](Individual *a, Individual *b) {
            return a->fitness > b->fitness;
        });

        // Selecciona los individuos más aptos para la siguiente generación
        std::vector<Individual*> best = bestIndividuals();
        
        // Crea la nueva generación a partir de los individuos seleccionados
        std::vector<Individual*> nextGen = nextGeneration();
        
        // Reemplaza la población actual por la nueva generación
        individuals = nextGen;
        
        // Incrementa el contador de generaciones
        generation++;
    }

    // Sincroniza los resultados entre los procesos
    MPI_Barrier(MPI_COMM_WORLD);

    // Envía la nueva población a cada proceso
    MPI_Bcast(&individuals[0], population, MPI_INT, 0, MPI_COMM_WORLD);
}


std::vector<Individual*> Genetic::nextGeneration()
{
    std::vector<Individual*> newGeneration(individuals.size());
    std::vector<Individual*> best = bestIndividuals();
    
    // Perform elitism, best individuals pass directly to next generation
    for(int i = 0;i<best.size()/2;i++)
    {
        Individual *elite = createRandomIndividual();
        elite->mlp->setWeights(best[i]->mlp->getWeights());
        elite->mlp->setConnections(best[i]->mlp->getConnections());
        newGeneration[i] = elite;
    }
    
    // The remaining indiviuals are combination of two random individuals from the best
    for(int i = best.size()/2; i<individuals.size();i++)
    {
        Individual *child = createRandomIndividual();
        int a = randomNumber(0.0, 1.0) * (best.size()-1);
        int b = randomNumber(0.0, 1.0) * (best.size()-1);
        Individual *parent1 = best[a];
        Individual *parent2 = best[b];
        
        parent1->mate(*parent2, child);
        
        newGeneration[i] = child;  
    }
    
    return newGeneration;
}

std::vector<Individual*> Genetic::bestIndividuals()
{
    for(int i=0;i<individuals.size();i++)
        individuals[i]->calculateFitness();

    sort(individuals.begin(), individuals.end(), [](Individual *a, Individual *b)
    {
        return a->fitness > b->fitness;
    });
    
    for(int i = 0;i<10;i++)
        std::cout<<individuals[i]->fitness<<"\t";
    std::cout<<std::endl;
    
    return std::vector<Individual*>(individuals.begin(), individuals.begin() + ((int)(individuals.size()*0.2)));
}

