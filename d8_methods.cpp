#include "data_structures.h"
#include "utility.h"
#include "d8_methods.h"
#include "interface.h"
#include <omp.h>
#include <queue>



int d8_FlowDir(const float_2d &elevations, const int x, const int y){
	float minimum_elevation=elevations(x,y);
	int flowdir=NO_FLOW;

	if (EDGE_GRID(x,y,elevations.width(),elevations.height())) return NO_FLOW;
	if (elevations(x,y)==elevations.no_data) return d8_NO_DATA; //No data for this cell

	for(int n=1;n<=8;n++){
		if(!IN_GRID(x+dx[n],y+dy[n],elevations.width(),elevations.height())) continue;
		if(elevations(x+dx[n],y+dy[n])==elev_NO_DATA) continue;

		if(	elevations(x+dx[n],y+dy[n])<minimum_elevation || (elevations(x+dx[n],y+dy[n])==minimum_elevation && flowdir>0 && flowdir%2==0 && n%2==1) ){
			minimum_elevation=elevations(x+dx[n],y+dy[n]);
			flowdir=n;
		}
	}

	return flowdir;
}

int d8_flow_directions(const float_2d &elevations, char_2d &flowdirs, bool init){
	diagnostic_arg("The D8 flow directions will require approximately %ldMB of RAM.\n",elevations.width()*elevations.height()*sizeof(char)/1024/1024);
	diagnostic("Resizing flow directions matrix...");
	flowdirs.resize(elevations.width(),elevations.height(),!init);
	diagnostic("succeeded.\n");

	if(init){
		diagnostic("Setting no_data value on flowdirs matrix...");
		flowdirs.no_data=d8_NO_DATA;
		diagnostic("succeeded.\n");

		diagnostic("Initializing D8 flow directions...\n");
		progress_bar(-1);
		#pragma omp parallel for
		for(int x=0;x<elevations.width();x++){
			progress_bar(x*omp_get_num_threads()*elevations.height()*100/(elevations.width()*elevations.height()));
			for(int y=0;y<elevations.height();y++)
				flowdirs(x,y)=NO_FLOW;
		}
		progress_bar(-1);
		diagnostic("\tsucceeded.\n");
	}

	diagnostic("Calculating D8 flow directions...\n");
	progress_bar(-1);
	#pragma omp parallel for
	for(int x=0;x<elevations.width();x++){
		progress_bar(x*omp_get_num_threads()*elevations.height()*100/(elevations.width()*elevations.height()));
		for(int y=0;y<elevations.height();y++)
			if(flowdirs(x,y)==NO_FLOW)
				flowdirs(x,y)=d8_FlowDir(elevations,x,y);
	}
	progress_bar(-1);
	diagnostic("\tsucceeded.\n");
}








//234
//105
//876
bool does_cell_flow_into_me(const int x, const int y, int n, const char_2d &flowdirs){
	return (flowdirs(x,y)!=d8_NO_DATA && n==inverse_flow[flowdirs(x,y)]);
}

int d8_upslope_area(const char_2d &flowdirs, uint_2d &area){
	char_2d dependency;
	std::queue<grid_cell> sources;

	diagnostic_arg("The sources queue will require at most approximately %ldMB of RAM.\n",flowdirs.width()*flowdirs.height()*sizeof(grid_cell)/1024/1024);

	diagnostic_arg("The dependency matrix will require approximately %ldMB of RAM.\n",flowdirs.width()*flowdirs.height()*sizeof(char)/1024/1024);
	diagnostic("Resizing dependency matrix...");
	dependency.resize(flowdirs.width(),flowdirs.height(),false);
	diagnostic("succeeded.\n");

	diagnostic_arg("The area matrix will require approximately %ldMB of RAM.\n",flowdirs.width()*flowdirs.height()*sizeof(unsigned int)/1024/1024);
	diagnostic("Resizing the area matrix...");
	area.resize(flowdirs.width(),flowdirs.height(),false);
	diagnostic("succeeded.\n");

	diagnostic("Calculating dependency matrix...\n");
	progress_bar(-1);
	#pragma omp parallel for
	for(int x=0;x<flowdirs.width();x++){
		progress_bar(x*omp_get_num_threads()*flowdirs.height()*100/(flowdirs.width()*flowdirs.height()));
		for(int y=0;y<flowdirs.height();y++){
			dependency(x,y)=0;
			for(int n=1;n<=8;n++){
				if(!IN_GRID(x+dx[n],y+dy[n],flowdirs.width(),flowdirs.height())) continue;
				if(does_cell_flow_into_me(x+dx[n],y+dy[n],n,flowdirs))
					dependency(x,y)++;
			}
		}
	}
	progress_bar(-1);
	diagnostic("\tsucceeded.\n");

	diagnostic("Locating source cells...\n");
	progress_bar(-1);
	for(int x=0;x<flowdirs.width();x++){
		progress_bar(x*omp_get_num_threads()*flowdirs.height()*100/(flowdirs.width()*flowdirs.height()));
		for(int y=0;y<flowdirs.height();y++)
			if(flowdirs(x,y)!=d8_NO_DATA && dependency(x,y)==0)
				sources.push(grid_cell(x,y));
	}
	progress_bar(-1);
	diagnostic("\tsucceeded.\n");

	diagnostic("Calculating up-slope areas...\n");
	progress_bar(-1);
	long int ccount=0;
	while(sources.size()>0){
		grid_cell c=sources.front();

		ccount++;
		progress_bar(ccount*100/flowdirs.data_cells);

		area(c.x,c.y)=1;
		for(int n=0;n<8;n++){
			if(!IN_GRID(c.x+dx[n],c.y+dy[n],flowdirs.width(),flowdirs.height())) continue;
			if(does_cell_flow_into_me(c.x+dx[n],c.y+dy[n],n,flowdirs))
				area(c.x,c.y)+=area(c.x+dx[n],c.y+dy[n]);
		}
		if(flowdirs(c.x,c.y)!=d8_NO_DATA && flowdirs(c.x,c.y)!=0)
			if( (--dependency(c.x+dx[flowdirs(c.x,c.y)],c.y+dy[flowdirs(c.x,c.y)]))==0)
				sources.push(grid_cell(c.x+dx[flowdirs(c.x,c.y)],c.y+dy[flowdirs(c.x,c.y)]));

		sources.pop();
	}
	progress_bar(-1);
	diagnostic("\tsucceeded.\n");
}
