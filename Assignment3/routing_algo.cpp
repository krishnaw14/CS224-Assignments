#include <vector>
#include "node.h"
#include <iostream>
#include <string>

using namespace std;

void routingAlgo(vector<RoutingNode*> nd)
{
	int count=nd.size();
	while(count--)
	{
		for(int i=0;i<nd.size();i++)
			nd[i]->sendMsg();
	}
}

void RoutingNode::recvMsg(RouteMsg *msg) 
{
	for(int i=0;i<msg->mytbl->tbl.size();i++)
	{
		int count=0,alpha;
		//To check if an entry of sender's table is there in receiver's table
		for(int j=0;j<mytbl.tbl.size();j++)
		{
			if(mytbl.tbl[j].dstip==msg->mytbl->tbl[i].dstip)
			{
				alpha=j;
				count++;
				break;
			}
		}
		if(count==0)
		{
			RoutingEntry new_route;
			new_route.ip_interface= msg->recvip;
			new_route.nexthop=msg->from;
			new_route.dstip=msg->mytbl->tbl[i].dstip;
			if(msg->from==new_route.dstip)
			{
				new_route.cost=1;
			}
			else
				new_route.cost=1+msg->mytbl->tbl[i].cost;
			mytbl.tbl.push_back(new_route);
		}
		else
		{
			if(mytbl.tbl[alpha].cost > msg->mytbl->tbl[i].cost+1)
				mytbl.tbl[alpha].cost=msg->mytbl->tbl[i].cost+1;
		}

	}
}