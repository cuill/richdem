#ifndef _richdem_wei2008_hpp_
#define _richdem_wei2008_hpp_

#include <richdem/common/Array2D.hpp>
#include <richdem/common/grid_cell.hpp>
#include <algorithm>
#include <iostream>
#include <queue>
#include <vector>


namespace richdem {



template<class T>
void InitPriorityQue(
  Array2D<T>& dem,
  Array2D<bool>& flag,
  GridCellZ_pq<T>& priorityQueue
){
  std::queue<GridCellZ<T> > depressionQue;

  // push border cells into the PQ
  for(int y = 0; y < dem.height(); y++)
  for(int x = 0; x < dem.width(); x++){
    if (flag(x,y)) continue;

    if (dem.isNoData(x,y)) {
      flag(x,y)=true;
      for (int i=1;i<=8; i++){
        auto ny = y+dy[i];
        auto nx = x+dx[i];

        if(!dem.inGrid(nx,ny))
          continue;

        if (flag(nx,ny)) 
          continue;

        if (!dem.isNoData(nx, ny)){
          priorityQueue.emplace(nx,ny,dem(nx, ny));
          flag(nx,ny)=true;
        }
      }
    } else if(dem.isEdgeCell(x,y)){
      //on the DEM border
      priorityQueue.emplace(x,y,dem(x,y));
      flag(x,y)=true;          
    }
  }
}



template<class T>
void ProcessTraceQue(
  Array2D<T>& dem,
  Array2D<bool>& flag,
  std::queue<GridCellZ<T> >& traceQueue,
  GridCellZ_pq<T>& priorityQueue
){
  std::queue<GridCellZ<T>  > potentialQueue;
  int indexThreshold=2;  //index threshold, default to 2
  while (!traceQueue.empty()){
    const auto node = traceQueue.front();
    traceQueue.pop();
    bool Mask[5][5]={{false},{false},{false},{false},{false}};
    for (int i=1;i<=8; i++){
      auto ny = node.y+dy[i];
      auto nx = node.x+dx[i];
      if(flag(nx,ny))
        continue;

      if (dem(nx,ny)>node.z){
        traceQueue.emplace(nx,ny, dem(nx,ny));
        flag(nx,ny)=true;
      } else {
        //initialize all masks as false   
        bool have_spill_path_or_lower_spill_outlet=false; //whether cell i has a spill path or a lower spill outlet than node if i is a depression cell
        for(int k=1;k<=8; k++){
          auto kRow = ny+dy[k];
          auto kCol = nx+dx[k];
          if((Mask[kRow-node.y+2][kCol-node.x+2]) ||
            (flag(kCol,kRow)&&dem(kCol,kRow)<node.z)
            )
          {
            Mask[ny-node.y+2][nx-node.x+2]=true;
            have_spill_path_or_lower_spill_outlet=true;
            break;
          }
        }
        
        if(!have_spill_path_or_lower_spill_outlet){
          if (i<indexThreshold) potentialQueue.push(node);
          else
            priorityQueue.push(node);
          break; // make sure node is not pushed twice into PQ
        }
      }
    }//end of for loop
  }

  while (!potentialQueue.empty()){
    const auto node = potentialQueue.front();
    potentialQueue.pop();

    //first case
    for (int i=1;i<=8; i++){
      auto ny = node.y+dy[i];
      auto nx = node.x+dx[i];
      if(flag(nx,ny))
        continue;

      priorityQueue.push(node);
      break;
    }   
  }
}



template<class T>
void ProcessPit(
  Array2D<T>& dem, 
  Array2D<bool>& flag, 
  std::queue<GridCellZ<T> >& depressionQue,
  std::queue<GridCellZ<T> >& traceQueue,
  GridCellZ_pq<T>& priorityQueue
){
  while (!depressionQue.empty()){
    auto node = depressionQue.front();
    depressionQue.pop();
    for (int i=1;i<=8; i++){
      auto ny = node.y+dy[i];
      auto nx = node.x+dx[i];
      if (flag(nx,ny))
        continue;    

      auto iSpill = dem(nx,ny);
      if (iSpill > node.z){ //slope cell
        flag(nx,ny)=true;
        traceQueue.emplace(nx,ny,iSpill);
        continue;
      }

      //depression cell
      flag(nx,ny)=true;
      dem(nx, ny) = node.z;
      depressionQue.emplace(nx,ny,node.z);
    }
  }
}



template<class T>
void fillDEM(Array2D<T> &dem){
  std::queue<GridCellZ<T> > traceQueue;
  std::queue<GridCellZ<T> > depressionQue;
  
  std::cout<<"Using our proposed variant to fill DEM"<<std::endl;
  Array2D<bool> flag(dem.width(),dem.height(),false);

  GridCellZ_pq<T> priorityQueue;

  int numberofall   = 0;
  int numberofright = 0;

  InitPriorityQue(dem,flag,priorityQueue); 
  while (!priorityQueue.empty()){
    auto tmpNode = priorityQueue.top();
    priorityQueue.pop();
    auto row   = tmpNode.y;
    auto col   = tmpNode.x;
    auto spill = tmpNode.z;

    for (int i=1;i<=8; i++){
      auto ny = row+dy[i];
      auto nx = col+dx[i];

      if(!dem.inGrid(nx,ny))
        continue;

      if(flag(nx,ny))
        continue;

      auto iSpill = dem(nx,ny);
      if (iSpill <= spill){
        //depression cell
        dem(nx,ny) = spill;
        flag(nx,ny) = true;
        depressionQue.emplace(nx,ny,spill);
        ProcessPit(dem,flag,depressionQue,traceQueue,priorityQueue);
      } else {
        //slope cell
        flag(nx,ny) = true;
        traceQueue.emplace(nx,ny,iSpill);
      }     
      ProcessTraceQue(dem,flag,traceQueue,priorityQueue); 
    }
  }
}

}

#endif
