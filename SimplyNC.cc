#include <vector>
#include <stdint.h>
#include <algorithm>
#include "GaloisField.h"
#include "GaloisFieldElement.h"
#include "GaloisFieldPolynomial.h"
#include "GeneratorPolynomialCreator.h"
//test git
#include <time.h>
//#include "globVar.h"

using namespace std;
using namespace galois;

#ifdef NS3_HACK
#include "GaloisField.cc"
#include "GaloisFieldElement.cc"
#include "GaloisFieldPolynomial.cc"
#endif
#define TOTAL_NODES 100
#define BATCH_SIZE 32
#define CODING_SPACE 254
#define TIME_OUT 5
#define NUMBER_OF_HASH 1

unsigned int prim_poly[9] = {1,1,1,0,0,0,0,1,1};
GaloisField gf(8,prim_poly);

	struct Forwarder_s{
		uint16_t id;
		uint16_t credit;
	};

struct PacketMORE_s
	{
		uint32_t packetType ; // CCACK should not have packet type. But better keep it 
		// so that it can be used to distinguish between control packet and CCACK packet.
		uint16_t srcAddress;
		uint16_t destAddress;
		uint32_t flowID;
		uint32_t batchNumber;
		uint8_t codeVector[BATCH_SIZE];
		uint32_t numberOfFwders;
		vector<Forwarder_s> fwdList;
	};

struct PacketCCACK_s
	{
		uint32_t packetType ; // CCACK should not have packet type. But better keep it 
		// so that it can be used to distinguish between control packet and CCACK packet.
		uint16_t srcAddress;
		uint16_t destAddress;
		uint32_t flowID;
		uint32_t batchNumber;
		uint32_t rate;
		uint8_t codeVector[BATCH_SIZE];
		uint8_t ACKcodeVector[BATCH_SIZE];
		uint32_t numberOfGlobalFwders;
		vector<Forwarder_s> fwdListGlobal;
		uint32_t numberOfLocalFwders;
		vector<Forwarder_s> fwdListLocal;
	};
struct chunkInfo{
		vector <uint8_t> codeVector;
		bool heard;
		int usageCount;
	};

struct neighbour_info{
	    int node_id;
		int last_credit;
		int last_time;
	};


class Flow{
public:


	uint32_t flowId;
	PacketCCACK_s CCACKpacketInfo;
	PacketMORE_s MOREpacketInfo;
		
	//Update just_received whenever we get a new packet;
	//vector< vector<galois::GaloisFieldElement> > list_chunks_, just_received_;
	vector <vector <uint8_t> > list_chunks_;
	vector <vector <galois::GaloisFieldElement> > phi;
	vector <vector <galois::GaloisFieldElement> > inde_heard_acks;
		vector<Forwarder_s> list_GlobalNeighbors_;
	vector<Forwarder_s> list_LocalNeighbors_;
	vector <chunkInfo> received_chunks, transmit_chunks,bin_chunks;
	int flow_credit;
	float relative_flow_counter;
	uint32_t batchId;
	int new_comb_;// This keep the track of new packets arrived..
	int Btx_marked,Brx_marked,Bin_marked;
	Flow(uint32_t gid):flowId(gid),batchId(0){};
	Flow(uint32_t gid,uint32_t gBatchId):flowId(gid),batchId(gBatchId){};
	~Flow(){};

    
	int ack_vector_check(uint8_t* A, uint8_t* B,int size)
{
	int result = 0; 
	for(int i=0;i<32;i++)
	{
		result += A[i]*B[i];
	}
	
	return result;

}
	
	void update_ack_counters(PacketCCACK_s pkt)
	{
		cout<<"start : Update counters\n";
		int i = 0,result;
		for(i = 0;(unsigned int)i < received_chunks.size();i++ )
		{
		result = ack_vector_check(&received_chunks[i].codeVector[0],pkt.ACKcodeVector,32);
		if(!result)
		{
			received_chunks[i].heard = 1;
			Brx_marked++;
		}
		}

		for(i = 0;(unsigned int)i < transmit_chunks.size();i++ )
		{
			result = ack_vector_check(&transmit_chunks[i].codeVector[0],pkt.ACKcodeVector,32);
			if(!result)
			{
				transmit_chunks[i].heard = 1;
				Btx_marked++;
			}	
		}
	cout<<"end : Update counters\n";
	}

    

	void  addGlobalForwarder(uint16_t id, uint16_t credit) // need to add flow too.. currently single flow
	{
		Forwarder_s newNeighbor;
		newNeighbor.id=id;
		newNeighbor.credit=credit;
		list_GlobalNeighbors_.push_back(newNeighbor);
	}

		void  addLocalForwarder(vector<Forwarder_s> fwder) // need to add flow too.. currently single flow
	{
		//Forwarder_s newNeighbor;
		//newNeighbor.id=id;
		//newNeighbor.credit=credit;
		list_LocalNeighbors_ = fwder;
	}

		
	void clearGlobalForwarder()
	{
		list_GlobalNeighbors_.clear();
	}

	void clearLocalForwarder()
	{
		list_LocalNeighbors_.clear();
	}

	uint32_t getDestId()
	{
		uint32_t destId=flowId >> 16 ;
		return destId;
	};

	void updatePacket(PacketCCACK_s &recvPacket)
	{
		CCACKpacketInfo=recvPacket;
	}
	void updatePacket(PacketMORE_s &recvPacket)
	{
		MOREpacketInfo=recvPacket;
	}

	bool checkSameBatch(uint32_t gBatchId)
	{
		if ( batchId == gBatchId)
		{
			return true;
		}
		else
		{
			// Clearing the chunks. As we have probably moved to the next batch. 
			list_chunks_.clear();
			received_chunks.clear();
			transmit_chunks.clear();
			return false;
		}
	}

	void addToTransmitPacket(uint8_t *pCodeVector)
	{
		vector<uint8_t> txCodeVector(BATCH_SIZE);
		for (int i=0; i< BATCH_SIZE ; i++)
		{
			txCodeVector.at(i)=pCodeVector[i];
		}
		chunkInfo temp;
		temp.codeVector=txCodeVector;
		temp.heard=false;
		transmit_chunks.push_back(temp);

	}

	int ACKindependence_(vector<GaloisFieldElement> &u) {
  if (this->phi.size() == 0) {
    return 1;
  }
  int i, j, p, q;
  const int m=this->phi.size()+1, n=BATCH_SIZE;
  vector< vector<galois::GaloisFieldElement> > check_list_;
  for (i=0; i<m-1; i++) {
    check_list_.push_back(this->phi[i]);
  }
  check_list_.push_back(u);
  i=0;
  j=0;

  while ((i < m) && (j < n) ) {
    galois::GaloisFieldElement max_val = check_list_[i][j];
    int max_ind = i;
    int k;
    if (max_val == 0) {
      for (k=i+1; k<m; k++) {
	galois::GaloisFieldElement val = check_list_[k][j];
	if (val > max_val) {
	  max_val = val;
	  max_ind = k;
	  break;
	}
      }
    }
    if (max_val != 0) {
      if (max_ind != i) {
 	vector< vector<galois::GaloisFieldElement> > temp_;
 	temp_.push_back(check_list_[i]);
 	temp_.push_back(check_list_[max_ind]);

 	for (p=0; p<n; p++) {
 	  check_list_[max_ind][p] = temp_[0][p];
 	  check_list_[i][p] = temp_[1][p];
 	}
      }
      for (q=i+1; q<m; q++) {
	galois::GaloisFieldElement toto_ = check_list_[q][j];
	for (p=j; p<n; p++) {
	  galois::GaloisFieldElement tata_ = check_list_[q][p];
	  check_list_[q][p] = tata_ - toto_*check_list_[i][p]/check_list_[i][j];
	}
      }
      i = i + 1;
    }
    j = j + 1;
  }
  int rank_ = 0;
  for (i=0; i<m; i++) {
    for (j=0; j<n; j++) {
      if (check_list_[i][j] != 0) {
	rank_++; 
	break;
      }
    }
  }
  if (rank_ == m) {
    return 1;
  }
  else {
    return 0;
  }
};


int marked_ack_independence_(vector<GaloisFieldElement> &u) {
  if (this->inde_heard_acks.size() == 0) {
    return 1;
  }
  int i, j, p, q;
  const int m=this->phi.size()+1, n=BATCH_SIZE;
  vector< vector<galois::GaloisFieldElement> > check_list_;
  for (i=0; i<m-1; i++) {
    check_list_.push_back(this->inde_heard_acks[i]);
  }
  check_list_.push_back(u);
  i=0;
  j=0;

   while ((i < m) && (j < n) ) {
    galois::GaloisFieldElement max_val = check_list_[i][j];
    int max_ind = i;
    int k;
    if (max_val == 0) {
      for (k=i+1; k<m; k++) {
	galois::GaloisFieldElement val = check_list_[k][j];
	if (val > max_val) {
	  max_val = val;
	  max_ind = k;
	  break;
	}
      }
    }
    if (max_val != 0) {
      if (max_ind != i) {
 	vector< vector<galois::GaloisFieldElement> > temp_;
 	temp_.push_back(check_list_[i]);
 	temp_.push_back(check_list_[max_ind]);

 	for (p=0; p<n; p++) {
 	  check_list_[max_ind][p] = temp_[0][p];
 	  check_list_[i][p] = temp_[1][p];
 	}
      }
      for (q=i+1; q<m; q++) {
	galois::GaloisFieldElement toto_ = check_list_[q][j];
	for (p=j; p<n; p++) {
	  galois::GaloisFieldElement tata_ = check_list_[q][p];
	  check_list_[q][p] = tata_ - toto_*check_list_[i][p]/check_list_[i][j];
	}
      }
      i = i + 1;
    }
    j = j + 1;
  }
  int rank_ = 0;
  for (i=0; i<m; i++) {
    for (j=0; j<n; j++) {
      if (check_list_[i][j] != 0) {
	rank_++; 
	break;
      }
    }
  }
  if (rank_ == m) {
    return 1;
  }
  else {
    return 0;
  }
};

	//int ACKindependence_(vector<uint8_t> &justReceived)
	//{

	//	cout << " Innovative List Chunk Size " <<  list_chunks_.size() << endl;
	//	for (int j=0; (unsigned int) j < list_chunks_.size() ; j++)
	//	{
	//		vector<uint8_t >InnovativeCodeVector=list_chunks_.at(j);
	//		for (int i=0 ; i <BATCH_SIZE ;i++)
 //              {
 //                      cout <<"InnovativeCodeVector " << i << ": " <<InnovativeCodeVector[i] << endl;

 //              }
	//	}
	//	if (list_chunks_.size() == 0) {
	//		return 1;
	//	}

	//	//TAMAL: Tricky.. but should be fine
	//	// create 2 new vector< vecotr <galois::GaloisFieldElement >> for the funciton.


	//	int i, j, p, q;
	//	const int m=list_chunks_.size()+1, n=BATCH_SIZE;
	//	vector< vector<galois::GaloisFieldElement> > check_list_;


	//	for (i=0; i<m-1; i++) {
	//		vector<galois::GaloisFieldElement> galoisListChunk;
	//		for(int x=0; x < BATCH_SIZE ; x++)
	//		{
	//			galois::GaloisFieldElement temp(&gf,int(list_chunks_[i][x]));
	//			galoisListChunk.push_back(temp);
	//		}
	//		check_list_.push_back(galoisListChunk);
	//	}
	//	vector<galois::GaloisFieldElement> galoisJustReceived;
	//	for(int x=0; x < BATCH_SIZE ; x++)
	//		{
	//			galois::GaloisFieldElement temp(&gf,int(justReceived[x]));
	//			galoisJustReceived.push_back(temp);
	//		}
	//	check_list_.push_back(galoisJustReceived);
	//	i=0;
	//	j=0;

	//	while ((i < m) && (j < n) ) {
	//		galois::GaloisFieldElement max_val = check_list_[i][j];
	//		int max_ind = i;
	//		int k;
	//		if (max_val == 0) {
	//			for (k=i+1; k<m; k++) {
	//				galois::GaloisFieldElement val = check_list_[k][j];
	//				if (val > max_val) {
	//					max_val = val;
	//					max_ind = k;
	//					break;
	//				}
	//			}
	//		}
	//		if (max_val != 0) {
	//			if (max_ind != i) {
	//				vector< vector<galois::GaloisFieldElement> > temp_;
	//				temp_.push_back(check_list_[i]);
	//				temp_.push_back(check_list_[max_ind]);

	//				for (p=0; p<n; p++) {
	//					check_list_[max_ind][p] = temp_[0][p];
	//					check_list_[i][p] = temp_[1][p];
	//				}
	//			}
	//			for (q=i+1; q<m; q++) {
	//				galois::GaloisFieldElement toto_ = check_list_[q][j];
	//				for (p=j; p<n; p++) {
	//					galois::GaloisFieldElement tata_ = check_list_[q][p];
	//					check_list_[q][p] = tata_ - toto_*check_list_[i][p]/check_list_[i][j];
	//				}
	//			}
	//			i = i + 1;
	//		}
	//		j = j + 1;
	//	}
	//	int rank_ = 0;
	//	for (i=0; i<m; i++) {
	//		for (j=0; j<n; j++) {
	//			if (check_list_[i][j] != 0) {
	//				rank_++; 
	//				break;
	//			}
	//		}
	//	}
	//	if (rank_ == m) {
	//		return 1;
	//	}
	//	else {
	//		return 0;
	//	}
	//};


	int independence_(vector<uint8_t> &justReceived)
	{

		cout << " Innovative List Chunk Size " <<  list_chunks_.size() << endl;
		for (int j=0; (unsigned int) j < list_chunks_.size() ; j++)
		{
			vector<uint8_t >InnovativeCodeVector=list_chunks_.at(j);
			for (int i=0 ; i <BATCH_SIZE ;i++)
               {
                       cout <<"InnovativeCodeVector " << i << ": " <<InnovativeCodeVector[i] << endl;

               }
		}
		if (list_chunks_.size() == 0) {
			return 1;
		}

		//TAMAL: Tricky.. but should be fine
		// create 2 new vector< vecotr <galois::GaloisFieldElement >> for the funciton.


		int i, j, p, q;
		const int m=list_chunks_.size()+1, n=BATCH_SIZE;
		vector< vector<galois::GaloisFieldElement> > check_list_;


		for (i=0; i<m-1; i++) {
			vector<galois::GaloisFieldElement> galoisListChunk;
			for(int x=0; x < BATCH_SIZE ; x++)
			{
				galois::GaloisFieldElement temp(&gf,int(list_chunks_[i][x]));
				galoisListChunk.push_back(temp);
			}
			check_list_.push_back(galoisListChunk);
		}
		vector<galois::GaloisFieldElement> galoisJustReceived;
		for(int x=0; x < BATCH_SIZE ; x++)
			{
				galois::GaloisFieldElement temp(&gf,int(justReceived[x]));
				galoisJustReceived.push_back(temp);
			}
		check_list_.push_back(galoisJustReceived);
		i=0;
		j=0;

		while ((i < m) && (j < n) ) {
			galois::GaloisFieldElement max_val = check_list_[i][j];
			int max_ind = i;
			int k;
			if (max_val == 0) {
				for (k=i+1; k<m; k++) {
					galois::GaloisFieldElement val = check_list_[k][j];
					if (val > max_val) {
						max_val = val;
						max_ind = k;
						break;
					}
				}
			}
			if (max_val != 0) {
				if (max_ind != i) {
					vector< vector<galois::GaloisFieldElement> > temp_;
					temp_.push_back(check_list_[i]);
					temp_.push_back(check_list_[max_ind]);

					for (p=0; p<n; p++) {
						check_list_[max_ind][p] = temp_[0][p];
						check_list_[i][p] = temp_[1][p];
					}
				}
				for (q=i+1; q<m; q++) {
					galois::GaloisFieldElement toto_ = check_list_[q][j];
					for (p=j; p<n; p++) {
						galois::GaloisFieldElement tata_ = check_list_[q][p];
						check_list_[q][p] = tata_ - toto_*check_list_[i][p]/check_list_[i][j];
					}
				}
				i = i + 1;
			}
			j = j + 1;
		}
		int rank_ = 0;
		for (i=0; i<m; i++) {
			for (j=0; j<n; j++) {
				if (check_list_[i][j] != 0) {
					rank_++; 
					break;
				}
			}
		}
		if (rank_ == m) {
			return 1;
		}
		else {
			return 0;
		}
	};
	

};
class NC_node{
public:

	const uint16_t id;
	uint32_t currBatch;
	NC_node(uint16_t gid):id(gid),currBatch(0){}; //currBatch,Forwarders list
	~NC_node(){};
	vector <vector <uint8_t> >  just_received_;
	vector <Forwarder_s> local_fwdrs;
	int total_credit_out /*credit to be transmitted out*/;
 	int total_credit_in /*credit received from neighbours*/;
	vector<neighbour_info> neighbours;
	struct Flows_s
	{
		uint32_t flowId;
		Flow *pFlow; 
	};

	vector <Flows_s> flows;



	uint32_t getFlowId(uint16_t destId)
	{
		uint32_t flowId= uint32_t(id + pow(double(2),16)*destId);
		return flowId;
	}
	uint32_t getFlowId(uint16_t srcId, uint16_t destId)
	{
		uint32_t flowId= uint32_t(srcId + pow(double(2),16)*destId);
		return flowId;
	}

	void printPacketMORE(PacketMORE_s &recvPacket)
     	  {
               cout <<"packetType " <<recvPacket.packetType <<endl;
               cout <<"SourceID " <<recvPacket.srcAddress <<endl;
               cout <<"DestID " <<recvPacket.destAddress <<endl;
               cout <<"FlowID " <<recvPacket.flowID <<endl;

               cout <<"batchNumber " <<recvPacket.batchNumber <<endl;
               cout <<"# of Forwarders" <<recvPacket.numberOfFwders <<endl;
               for( unsigned int i=0; i< recvPacket.numberOfFwders ; i++)
               {
                       cout <<"Forwarder " << i << ":ID: " << recvPacket.fwdList.at(i).id << endl;
                       cout <<"Forwarder " << i << ":Credit: " << recvPacket.fwdList.at(i).credit << endl;

               }
               for (int i=0 ; i <BATCH_SIZE ;i++)
               {
                       cout <<"CodeVector " << i << ": " <<recvPacket.codeVector[i] << endl;

               }
               

      	 }

		void printPacketCCACK(PacketCCACK_s &recvPacket)
     	  {
               cout <<"packetType " <<recvPacket.packetType <<endl;
               cout <<"SourceID " <<recvPacket.srcAddress <<endl;
               cout <<"DestID " <<recvPacket.destAddress <<endl;
               cout <<"FlowID " <<recvPacket.flowID <<endl;

               cout <<"batchNumber " <<recvPacket.batchNumber <<endl;
               cout <<"# of Global Forwarders" <<recvPacket.numberOfGlobalFwders <<endl;
               for( unsigned int i=0; i< recvPacket.numberOfGlobalFwders ; i++)
               {
                       cout <<"Forwarder " << i << ":ID: " << recvPacket.fwdListGlobal.at(i).id << endl;
                       cout <<"Forwarder " << i << ":Credit: " << recvPacket.fwdListGlobal.at(i).credit << endl;

               }

			   cout <<"# of Local Forwarders" <<recvPacket.numberOfLocalFwders <<endl;
               for( unsigned int i=0; i< recvPacket.numberOfLocalFwders ; i++)
               {
                       cout <<"Forwarder " << i << ":ID: " << recvPacket.fwdListLocal.at(i).id << endl;
                       cout <<"Forwarder " << i << ":Credit: " << recvPacket.fwdListLocal.at(i).credit << endl;

               }
			    cout <<"CodeVector ";
               for (int i=0 ; i <BATCH_SIZE ;i++)
               {
                       cout <<": " <<recvPacket.codeVector[i]  ;

               }
			   cout <<endl;
			   cout <<"ACK CodeVector ";
			for (int i=0 ; i <BATCH_SIZE ;i++)
               {
                       cout << ": " <<recvPacket.ACKcodeVector[i] ;

               }
					cout <<endl;
               

      	 }

	void newPacketBySource(uint16_t destId)
	{
		uint32_t flowId=getFlowId(destId);
		bool matching=false;
	
		Flow *pFlow=NULL;
		for(int i=0; (unsigned int)i< flows.size() ; i++)
		{
			if (flowId == flows.at(i).flowId)
			{
				matching=true;
				pFlow=flows.at(i).pFlow;
				break;
			}
		}
		if ( matching == false)
		{
			Flows_s newFlow;
			newFlow.flowId=flowId;
			newFlow.pFlow = new Flow(flowId);

			flows.push_back(newFlow);
			pFlow=newFlow.pFlow;
		}
		for( int numOfChunk=0; numOfChunk < BATCH_SIZE ; numOfChunk++)
		{
			vector<uint8_t> chunk;
			for( int i=0; i< BATCH_SIZE ; i++)
			{
				uint8_t x=uint8_t(rand() % (BATCH_SIZE));
				chunk.push_back(x);
			}

			addInnovativeChunk(pFlow,chunk);
		}

	}





		// Pass reference of rawPackSize, encode CCACKPacket will fill it up.
	//PacketCCACK_s pass by ref
	uint8_t * encodeMOREPacket(PacketMORE_s &pacInfo ,int &rawPackSize)
	{
		rawPackSize= 20 + BATCH_SIZE + sizeof(Forwarder_s)*pacInfo.numberOfFwders;
		uint8_t *pRawPacket= (uint8_t *)malloc(sizeof(uint8_t)*rawPackSize);
		memcpy(pRawPacket,&pacInfo.packetType,4);
		memcpy(pRawPacket+4,&pacInfo.srcAddress,2);
		memcpy(pRawPacket+6,&pacInfo.destAddress,2);
		memcpy(pRawPacket+8,&pacInfo.flowID,4);
		memcpy(pRawPacket+12,&pacInfo.batchNumber,4);
		memcpy(pRawPacket+16,pacInfo.codeVector,32);
		memcpy(pRawPacket+48,&pacInfo.numberOfFwders,4);


		for( int i=0;(unsigned int) i< pacInfo.numberOfFwders ; i++)
		{
			memcpy(pRawPacket+52+i*4,&pacInfo.fwdList[i].id,2);
			memcpy(pRawPacket+52+i*4+2,&pacInfo.fwdList[i].credit,2);
		}
		return pRawPacket;
	};

	// Pass size of the received packet
	//PacketCCACK_s pass by ref.
	void decodeMOREPacket( PacketMORE_s &pacInfo, uint8_t *pRawPacket, int rawPackSize)
	{
		

		memcpy(&pacInfo.packetType,pRawPacket,4);
		memcpy(&pacInfo.srcAddress,pRawPacket+4,2);
		memcpy(&pacInfo.destAddress,pRawPacket+6,2);
		memcpy(&pacInfo.flowID,pRawPacket+8,4);
		memcpy(&pacInfo.batchNumber,pRawPacket+12,4);
		memcpy(pacInfo.codeVector,pRawPacket+16,32);
		memcpy(&pacInfo.numberOfFwders,pRawPacket+48,4);
		
		for( int i=0; (unsigned int)i< pacInfo.numberOfFwders ; i++)
		{
			Forwarder_s temp;		
			memcpy(&temp.id,pRawPacket+52+i*4,2);
			memcpy(&temp.credit,pRawPacket+52+i*4+2,2);
			pacInfo.fwdList.push_back(temp);
		}

	};



	uint8_t * encodeCCACKPacket(PacketCCACK_s &pacInfo ,int &rawPackSize)
	{
		
		
		rawPackSize= 28 + 2*BATCH_SIZE + sizeof(Forwarder_s)*(pacInfo.numberOfGlobalFwders+pacInfo.numberOfLocalFwders);
		cout <<"Start : encode CCACK size\n" << rawPackSize ;
		uint8_t *pRawPacket= (uint8_t *)malloc(sizeof(uint8_t)*rawPackSize);
		memcpy(pRawPacket,&pacInfo.packetType,4);
		memcpy(pRawPacket+4,&pacInfo.srcAddress,2);
		memcpy(pRawPacket+6,&pacInfo.destAddress,2);
		memcpy(pRawPacket+8,&pacInfo.flowID,4);
		memcpy(pRawPacket+12,&pacInfo.batchNumber,4);
		memcpy(pRawPacket+16,&pacInfo.rate,4);
		memcpy(pRawPacket+20,pacInfo.codeVector,32);
		memcpy(pRawPacket+52,pacInfo.ACKcodeVector,32);
		memcpy(pRawPacket+84,&pacInfo.numberOfGlobalFwders,4);
		for( int i=0;(unsigned int) i< pacInfo.numberOfGlobalFwders ; i++)
		{
			memcpy(pRawPacket+88+i*4,&pacInfo.fwdListGlobal[i].id,2);
			memcpy(pRawPacket+88+i*4+2,&pacInfo.fwdListGlobal[i].credit,2);
		}
		int mid=88+pacInfo.numberOfGlobalFwders*4;
		memcpy(pRawPacket+mid,&pacInfo.numberOfLocalFwders,4);
		mid=mid+4;
		for( int i=0;(unsigned int) i< pacInfo.numberOfLocalFwders ; i++)
		{
			memcpy(pRawPacket+mid+i*4,&pacInfo.fwdListLocal[i].id,2);
			memcpy(pRawPacket+mid+i*4+2,&pacInfo.fwdListLocal[i].credit,2);
			cout<<"local forwaders " << pacInfo.fwdListLocal[i].id << endl;
		}
		//memcpy(pRawPacket,&pacInfo,rawPackSize);
		return pRawPacket;
	};


	/*Vishwas*/
	void update_credits_from_neighbour(PacketCCACK_s recvPacket)
	{
		int i; 
		neighbour_info temp;
		cout<<"start : adding neighbours\n";
		for(i = 0; (uint8_t)i < neighbours.size();i++)
		{
			if(neighbours[i].node_id == recvPacket.srcAddress)
			{
				neighbours[i].last_credit = recvPacket.srcAddress; /*this should be replaced by proper credit*/
				neighbours[i].last_time = time(NULL);
			}
		}
		if((unsigned int)i == neighbours.size())
		{
			temp.node_id = recvPacket.srcAddress;
			temp.last_time = time(NULL);
			temp.last_credit = recvPacket.srcAddress; /*to be replaced by proper credit*/
			neighbours.push_back(temp);
		}
		cout<<"end : adding neighbours\n";
	}

	
	// Pass size of the received packet
	//PacketCCACK_s pass by ref.
	void decodeCCACKPacket( PacketCCACK_s &pacInfo, uint8_t *pRawPacket, int rawPackSize)
	{
		
		memcpy(&pacInfo.packetType,pRawPacket,4);
		memcpy(&pacInfo.srcAddress,pRawPacket+4,2);
		memcpy(&pacInfo.destAddress,pRawPacket+6,2);
		memcpy(&pacInfo.flowID,pRawPacket+8,4);
		memcpy(&pacInfo.batchNumber,pRawPacket+12,4);
		memcpy(&pacInfo.rate,pRawPacket+16,4);
		memcpy(pacInfo.codeVector,pRawPacket+20,32);
		memcpy(pacInfo.ACKcodeVector,pRawPacket+52,32);
		memcpy(&pacInfo.numberOfGlobalFwders,pRawPacket+84,4);
		
		for( int i=0; (unsigned int)i< pacInfo.numberOfGlobalFwders ; i++)
		{
			Forwarder_s temp;		
			memcpy(&temp.id,pRawPacket+88+i*4,2);
			memcpy(&temp.credit,pRawPacket+88+i*4+2,2);
			pacInfo.fwdListGlobal.push_back(temp);
		}
		int mid=88+pacInfo.numberOfGlobalFwders*4;
		memcpy(&pacInfo.numberOfLocalFwders,pRawPacket+mid,4);
		mid=mid+4;
		for( int i=0; (unsigned int)i< pacInfo.numberOfLocalFwders ; i++)
		{
			Forwarder_s temp;		
			memcpy(&temp.id,pRawPacket+mid+i*4,2);
			memcpy(&temp.credit,pRawPacket+mid+i*4+2,2);
			pacInfo.fwdListLocal.push_back(temp);
		}
    
	

	};


	// Call this function when src want to add something new. or a node receive something innovative
	void addInnovativeChunk(Flow *pFlow, vector<uint8_t> chunk)
	{
		pFlow->list_chunks_.push_back(chunk);
	};

	//vishwas -
	        int check_credit(Flow *pFlow)
		{
			pFlow->relative_flow_counter += 1;
			if((0.83333*(float)pFlow->relative_flow_counter + 0.16666666) > 0)
				return 1;
			else
				return 0;
		} 
	
	
	uint8_t checkReceivedCCACK(uint8_t *pRawPacket, int rawPackSize)
	{

		PacketCCACK_s recvPacket;
		//TODO: need to create flowObject if not there.. do it later. 
		bool isForwarder=false;
		bool isDestination=false;
		bool readyToDecode=false;
		cout <<"before decoding\n";
		decodeCCACKPacket( recvPacket, pRawPacket,rawPackSize);
		cout <<"done decoding\n";
		printPacketCCACK(recvPacket);

		int numberGlobalForwarders=recvPacket.numberOfGlobalFwders;


		vector<uint8_t> justReceived(BATCH_SIZE);
		for(int i=0; i < BATCH_SIZE ; i++)
		{
			justReceived.at(i)=recvPacket.codeVector[i];

		}

		//just_received_.push_back(justReceived);

		bool matching=false;
		Flow *pFlow=NULL;
		for(int i=0;(unsigned int) i<flows.size() ; i++)
		{
			if(recvPacket.flowID == flows.at(i).flowId)
			{
				matching=true;
				pFlow=flows.at(i).pFlow;

				break;
			}
		}
		if( matching == false)
		{
			cout << "flow not found; Adding new flow" << endl;
			Flows_s newFlow;
			newFlow.flowId=recvPacket.flowID;
			newFlow.pFlow = new Flow(recvPacket.flowID,recvPacket.batchNumber);

			flows.push_back(newFlow);
			pFlow=newFlow.pFlow;
		}
		// pFlow should have the flowID now.

		//calling upatin ACK counters - vishwas
		pFlow->update_ack_counters(recvPacket);
		update_flow_credit(pFlow);
		/* update neighbours field */
		update_credits_from_neighbour(recvPacket);
			cout << "Useful" << endl;

			if(id == recvPacket.destAddress)
			{
				isDestination=true;
			}



			if( isDestination)
			{
				if( pFlow->independence_(justReceived))
				{

					pFlow->checkSameBatch(recvPacket.batchNumber);

					addInnovativeChunk(pFlow, justReceived);

					if(pFlow->list_chunks_.size() == BATCH_SIZE)
					{
						readyToDecode = true;
						if( readyToDecode == true)
						{

							//TODO: decide what to do.
							//It then sends an end-to-end ACK back to the source along the shortest ETX path in a reliable manner
						}

					}
				}


				cout << "of no good. Still keeping" << endl;
				addReceived(pFlow,justReceived);
			}



			else
			{
				for (int i = 0; i < numberGlobalForwarders ; i++)
				{
					if(id == recvPacket.fwdListGlobal[i].id)
					{
						isForwarder=true;
						break;
					}
				}
				if ( !isForwarder)
				{
					// Checking if there in local forwarder list
					int numberLocalForwarders=recvPacket.numberOfLocalFwders;
					for (int i = 0; i < numberLocalForwarders ; i++)
					{
						if(id == recvPacket.fwdListGlobal[i].id)
						{
							isForwarder=true;
							break;
						}
					}
				}

				if( isForwarder)
				{
					cout <<"forwarder found!!"<<endl;

				if( pFlow->independence_(justReceived))
				{

					addInnovativeChunk(pFlow, justReceived);
					//b). Hence, to address the collective space
			//problem, nodes need to remember all the packets that have been
			//in the air, not only the innovative ones.
					
				}
					cout << "of no good. Still keeping" << endl;
					addReceived(pFlow,justReceived);
				}
				else
				{
					//Do nothing
					//addReceived(justReceived);// TODO: check what should do.. when do we actually add in receive list. 

				}

			}
			if(isDestination || isForwarder)
			{
				pFlow->updatePacket(recvPacket);
			}

			//TODO: TAMAL: what should i do.. probably the best is to remove the flow from queue
			justReceived.clear();
				pFlow->new_comb_++;//TAMAL: see where to use
		//Vishwas - 
		int temp1 = 0;
		cout<<"checking for credit\n";
		for(int i=0;(unsigned int) i<flows.size() ; i++)
		{
			temp1 = check_credit(flows[i].pFlow);
			if(temp1)
			{
				cout<<"have credit- need to check contents of packet\n";
				//packet = CreateCCACKpakcet(9,flows[i].pFlow);
				return 1;
			}

		}

		return 0;
	}

	void checkReceivedMORE(uint8_t *pRawPacket, int rawPackSize)
	{

		PacketMORE_s recvPacket;
		//TODO: need to create flowObject if not there.. do it later. 
		bool isForwarder=false;
		bool isDestination=false;
		bool readyToDecode=false;
		decodeMOREPacket( recvPacket, pRawPacket,rawPackSize);
		cout <<"done decoding\n";
		printPacketMORE(recvPacket);

		int numberForwarders=recvPacket.numberOfFwders;


		vector<uint8_t> justReceived(BATCH_SIZE);
		for(int i=0; i < BATCH_SIZE ; i++)
		{
			justReceived.at(i)=recvPacket.codeVector[i];

		}

		//just_received_.push_back(justReceived);

		bool matching=false;
		Flow *pFlow=NULL;
		for(int i=0;(unsigned int) i<flows.size() ; i++)
		{
			if(recvPacket.flowID == flows.at(i).flowId)
			{
				matching=true;
				pFlow=flows.at(i).pFlow;

				break;
			}
		}
		if( matching == false)
		{
			cout << "flow not found; Adding new flow" << endl;
			Flows_s newFlow;
			newFlow.flowId=recvPacket.flowID;
			newFlow.pFlow = new Flow(recvPacket.flowID,recvPacket.batchNumber);

			flows.push_back(newFlow);
			pFlow=newFlow.pFlow;
		}
		// pFlow should have the flowID now.

		
		
			cout << "Useful" << endl;

			if(id == recvPacket.destAddress)
			{
				isDestination=true;
			}



			if( isDestination)
			{
				if( pFlow->independence_(justReceived))
				{

					pFlow->checkSameBatch(recvPacket.batchNumber);

					addInnovativeChunk(pFlow, justReceived);

					if(pFlow->list_chunks_.size() == BATCH_SIZE)
					{
						readyToDecode = true;
						if( readyToDecode == true)
						{

							//TODO: decide what to do.
							//It then sends an end-to-end ACK back to the source along the shortest ETX path in a reliable manner
						}

					}
				}


				cout << "of no good. Still keeping" << endl;
				addReceived(pFlow,justReceived);
			}



			else
			{
				for (int i = 0; i < numberForwarders ; i++)
				{
					if(id == recvPacket.fwdList[i].id)
					{
						isForwarder=true;
						break;
					}
				}

				if( isForwarder)
				{
					cout <<"forwarder found!!"<<endl;

				if( pFlow->independence_(justReceived))
				{

					addInnovativeChunk(pFlow, justReceived);
					//b). Hence, to address the collective space
			//problem, nodes need to remember all the packets that have been
			//in the air, not only the innovative ones.
					
				}
					cout << "of no good. Still keeping" << endl;
					addReceived(pFlow,justReceived);
				}
				else
				{
					//Do nothing
					//addReceived(justReceived);// TODO: check what should do.. when do we actually add in receive list. 

				}

			}
			if(isDestination || isForwarder)
			{
				pFlow->updatePacket(recvPacket);
			}

			//TODO: TAMAL: what should i do.. probably the best is to remove the flow from queue
			justReceived.clear();
				pFlow->new_comb_++;//TAMAL: see where to use
		

	};



	// This includes overHeard and innovative
	void addReceived(Flow *pFlow, vector<uint8_t> chunk)
	{
		chunkInfo temp;
		temp.codeVector=chunk;
		temp.heard=false;
		temp.usageCount=0;

		pFlow->received_chunks.push_back(temp);
//TODO: Check what bin_chunks should contain.
//TODO: function to check innovative heard packet
		pFlow->bin_chunks.push_back(temp);
		//TODO: use this information for ACK of CCACK
	};

	/*void calculate_flow_credit(Flow *pFlow) vishwas
	{
		int Bin = 0,Btx = 0,Brx = 0,i, pFlow_credit;
		
		pFlow->bin_chunks;
		for(i = 0; i  < pFlow->received_chunks.size(); i++){} ;//add it to a temp q ;/
		for(i = 0; i  < pFlow->transmit_chunks.size(); i++) ;//add it to temp q ;//check indepedence of heard packets
		for(i = 0; i  < pFlow->bin_chunks.size(); i++) Bin++;

		//check indepedence of heard packets
		//check indepedence
		//calculate pFlow credit

		
		cout <<"Bin " << Bin << "Brx " << Brx << "Btx " << Btx << endl;
		return(pFlow_credit);
		
		
    } */

	/*vishwas*/
	void calculate_node_out_credit()
    {
		unsigned int i;
		cout<<"calculating node credit out\n";
		total_credit_out = 0;
		for(i = 0; i < flows.size(); i++ )
		total_credit_out += flows[i].pFlow->flow_credit; /*flow_credit is updated after every packet is received*/
		cout<<"END: node credit out\n";
	}

	
	void calculate_node_in_credit() /*this function can be called every 2 seconds, to be decided later*/
	{
		unsigned int i,curr_time;
		curr_time = time(NULL);
		total_credit_in = 0;
		for(i = 0; i < neighbours.size(); i++)
		{
			if((neighbours[i].last_time - curr_time) < TIME_OUT)
			{
				total_credit_in = total_credit_in + neighbours[i].last_credit;
				cout <<"total credit updated\n";
			}

	    }
	}
	
    /*vishwas*/
	void update_relative_flow_counter(Flows_s *flow)
	{
    	flow->pFlow->relative_flow_counter = flow->pFlow->flow_credit / ( flow->pFlow->flow_credit + total_credit_in);
	} 

	/*Vishwas*/
	void update_flow_credit(Flow *pFlow)
	{
		cout<<"Start : Updating flow credit \n";
 		unsigned int i; //received_chunks, transmit_chunks,
		vector<GaloisFieldElement>  u;
		for(i = 0; i < pFlow->received_chunks.size(); i++)
		{
			if(pFlow->received_chunks[i].heard )
			{
				u = generateGaloisFieldFromCodeVector(pFlow->received_chunks.at(i).codeVector);
				if(pFlow->marked_ack_independence_(u) == 1)
				{
					pFlow->inde_heard_acks.push_back(u);
					pFlow->flow_credit++;
				}
			}
		}

		for(i = 0; i < pFlow->transmit_chunks.size(); i++)
		{	
			if(pFlow->received_chunks[i].heard )
			{
				u =generateGaloisFieldFromCodeVector(pFlow->transmit_chunks.at(i).codeVector);
				if(pFlow->marked_ack_independence_(u) == 1)
				{
					pFlow->inde_heard_acks.push_back(u);
					pFlow->flow_credit++;
				}
			}
		}
		cout<<"End : Updated flow credit to :" << pFlow->flow_credit<<endl;
	}
	
	void generateCodedData(uint8_t  *codeVector, Flow *pFlow)
	{
		//Use the vector of vector to make a linear combination of all the packets. 

		vector<uint8_t>  rand_coef_;
		vector<GaloisFieldElement>  galois_listChunk_;
		// If I have only one packet, no network coding needed.
		if (pFlow->list_chunks_.size() == 1) {
			for (int q=0; (unsigned int)q<pFlow->list_chunks_[0].size(); q++){ 
				codeVector[q]=pFlow->list_chunks_[0][q];
			}
		}
		else
		{

				for (int q=0;(unsigned int) q<pFlow->list_chunks_.size(); q++) {
					//
					//uint8_t tompo= uint8_t(rand() % (CODING_SPACE));
					uint8_t tompo = uint8_t(rand() % (CODING_SPACE+2));
					int temp1 = int(tompo);
					rand_coef_.push_back(temp1); // filling up random coeffs.. For each packet 1 random number
					
				}
				for (int k=0; k<BATCH_SIZE; k++) {
					galois::GaloisFieldElement tata_(&gf, 0);
					for (int q=0; (unsigned int)q<pFlow->list_chunks_.size(); q++) {
						//TAMAL: This is where the NETWORK CODING is done
						// The + operator is used for XORing the data with other coeff..
						galois::GaloisFieldElement tempRand_(&gf,rand_coef_[q]);
						galois::GaloisFieldElement chunk_gal(&gf, int(pFlow->list_chunks_[q][k]));
						tata_ = tata_ + tempRand_*chunk_gal;
					}
					codeVector[k]=tata_.poly();// Check.. Not very sure..
					cout<<"codeVector " << k << ": " << codeVector[k] << endl;
				}

		}
	};
	vector<GaloisFieldElement> generateGaloisFieldFromCodeVector ( vector <uint8_t> codeVector )
	{

		
			vector<GaloisFieldElement> galoisChunk;
			for(int x=0; x < BATCH_SIZE ; x++)
			{
				galois::GaloisFieldElement temp(&gf,int(codeVector[x]));
				galoisChunk.push_back(temp);
			}
			return galoisChunk;
	}

	vector<GaloisFieldElement> generateRandomGFelement()
	{
		vector<GaloisFieldElement> galoisChunk;
		for(int x=0; x < BATCH_SIZE ; x++)
			{
				galois::GaloisFieldElement temp(&gf,int(rand()% CODING_SPACE) +1 );
				galoisChunk.push_back(temp);
			}

		return galoisChunk;

	}

	vector<GaloisFieldElement> generateZeroGFelement(int k)
	{
		vector<GaloisFieldElement> galoisChunk;
		for(int x=0; x < k ; x++)
			{
				galois::GaloisFieldElement temp(&gf,0);
				galoisChunk.push_back(temp);
			}

		return galoisChunk;

	}
	
	void generateCodedACK(uint8_t  *codeVector, Flow *pFlow)
	{
		//TAMAL: Need to code CCACK HERE !!
		//Use the vector of vector to make a linear combination of all the packets. 



		pFlow->phi.clear();
		int receiveChunkSize=pFlow->received_chunks.size();
		if(receiveChunkSize == 0)
		{
			
			for( int i=0; i< BATCH_SIZE ; i++)
			{
				codeVector[i]=0;

			}

		}

		for(int loop=0; loop < receiveChunkSize ; loop++)
		{
			int minValue=0;
			int minCount=0;
			vector <int> minIndexList;
			for(int i=0; i<receiveChunkSize;i++)
			{
				if( minValue > pFlow->received_chunks.at(i).usageCount)
				{
					minValue=pFlow->received_chunks.at(i).usageCount;
					minIndexList.clear();
					minIndexList.push_back(i);
					minCount=1;

				}
				else if (minValue == pFlow->received_chunks.at(i).usageCount)
				{
					minIndexList.push_back(i);
					minCount++;

				}
				else
				{
					continue;
				}

			}

			int pickIndex=0;

			if( minCount > 1)
			{
				pickIndex=rand() % (minCount);

			}

			pickIndex=minIndexList.at(pickIndex); // Using pickIndexAgain, but to get index of received chunk


			vector<GaloisFieldElement> u =generateGaloisFieldFromCodeVector(pFlow->received_chunks.at(pickIndex).codeVector);
			if(pFlow->ACKindependence_(u) == 1)
			{
				pFlow->phi.push_back(u);
			}
			pFlow->received_chunks.at(pickIndex).usageCount++;


			if(pFlow->phi.size() >= (BATCH_SIZE/NUMBER_OF_HASH))
			{
				break;
			}

		}


		bool found=0;
		int foundSum;
		vector<GaloisFieldElement> randdomACK;
		while(found == 0)
		{
			foundSum=0;
			randdomACK=generateRandomGFelement();

			vector<GaloisFieldElement> output=generateZeroGFelement(pFlow->phi.size());

	


			for( int i=0; (unsigned int)i < pFlow->phi.size() ; i++)
			{
				
				for(int x=0; (unsigned int)x < BATCH_SIZE ; x++)
				{
					

					output.at(i)=output.at(i)+pFlow->phi.at(i).at(x)*randdomACK.at(x);
					
				}

				if(output.at(i).poly() != 0)
				{
					break;
				}
				else
				{
					foundSum++;
				}


			}
			if ( (unsigned int)foundSum == pFlow->phi.size())
			{
				break;
			}

		}

		//randdomACK contains our desired vector..

		for( int i=0; i< BATCH_SIZE ; i++)
		{
			codeVector[i]=randdomACK.at(i).poly();

		}


	};

	

	uint8_t *CreateMOREpakcet(uint16_t destId, Flow *pFlow)
	{
		//This API should actuall call addForwarder. But for simplicity currently making main to add forwarders list
		//addForwarder(5,10);

		PacketMORE_s makePacket;
		makePacket.numberOfFwders=pFlow->list_GlobalNeighbors_.size();
		makePacket.packetType=2;
		makePacket.srcAddress=id;
		makePacket.destAddress=destId;
		makePacket.flowID=getFlowId(destId); // I think this makes sense, to avoid confussion..
		makePacket.batchNumber=currBatch++;
		makePacket.fwdList=pFlow->list_GlobalNeighbors_;// TODO: need to put forwarders list in flow, not in node.
		generateCodedData(makePacket.codeVector,pFlow);
		pFlow->addToTransmitPacket(makePacket.codeVector);
		int packetSize=0;

		printPacketMORE(makePacket);
		uint8_t *pRawPakcet= encodeMOREPacket(makePacket,packetSize);
		
		return pRawPakcet;

	};

	uint8_t *CreateCCACKpakcet(uint16_t destId, Flow *pFlow)
	{
		//This API should actuall call addForwarder. But for simplicity currently making main to add forwarders list
		//addForwarder(5,10);
		cout<<"creating ccack packet\n";
		calculate_node_out_credit(); /*vishwas*/ //And Update
		
		PacketCCACK_s makePacket;
		makePacket.numberOfGlobalFwders=pFlow->list_GlobalNeighbors_.size();
		makePacket.numberOfLocalFwders=pFlow->list_LocalNeighbors_.size();
		makePacket.packetType=2;
		makePacket.srcAddress=id;
		makePacket.destAddress=destId;
		makePacket.flowID=getFlowId(destId); // I think this makes sense, to avoid confussion..
		makePacket.batchNumber=currBatch;
		makePacket.fwdListGlobal=pFlow->list_GlobalNeighbors_;// TODO: need to put forwarders list in flow, not in node.
		makePacket.fwdListLocal=pFlow->list_LocalNeighbors_;// TODO: need to put forwarders list in flow, not in node.
		makePacket.fwdListGlobal[0].credit = total_credit_out;//TODO: instead of fwdListGlobal[0] make the index correct
		generateCodedData(makePacket.codeVector,pFlow);
		generateCodedACK(makePacket.ACKcodeVector,pFlow);
		pFlow->addToTransmitPacket(makePacket.codeVector);
		int packetSize=0;
		pFlow->relative_flow_counter--; /*vishwas - updateing credit counter after listning*/
		printPacketCCACK(makePacket);
		uint8_t *pRawPakcet= encodeCCACKPacket(makePacket,packetSize);
		cout << "after creating packet \n";
		decodeCCACKPacket(makePacket,pRawPakcet,packetSize);
		printPacketCCACK(makePacket);
		return pRawPakcet;

	};
	

	//TAMAL: Just for testing .. remove later. 


	Flow *getFlow(uint16_t destId)
	{
		bool matching=false;
		uint32_t flowId=getFlowId(destId);
		Flow *pFlow=NULL;
		for(int i=0;(unsigned int) i< flows.size() ; i++)
		{
			if (flowId == flows.at(i).flowId)
			{
				matching=true;
				pFlow=flows.at(i).pFlow;
				break;
			}
		}
		if ( matching == false)
		{
			Flows_s newFlow;
			newFlow.flowId=flowId;
			newFlow.pFlow = new Flow(flowId);
			flows.push_back(newFlow);
			pFlow=newFlow.pFlow;
		}
		return pFlow;

						

	}
	uint8_t * sendBySource(Flow *pFlow )
	{
		uint16_t destId=pFlow->getDestId();

		// Now we have the flow ready.

		uint8_t *pRawPacket=NULL;
		
			pRawPacket=CreateCCACKpakcet(destId,pFlow);
			//TODO: call send subroutine
		
		return pRawPacket; // TAMAL: TODO: Does not make any sense now.. it can not return only one packet.. Just for tesing
							// remove it... the return type should be "void " for this case
							//TODO: free memory.. call free.. 

	};

	

		uint8_t *modifyCCACKpakcet(Flow *pFLow)
	{
		//This API should actuall call addForwarder. But for simplicity currently making main to add forwarders list
		//addForwarder(5,10);
		cout<<"start : modify CCACK packet\n";
		PacketCCACK_s makePacket=pFLow->CCACKpacketInfo;
		generateCodedData(makePacket.codeVector,pFLow);
		pFLow->addToTransmitPacket(makePacket.codeVector);
		int packetSize=0;
		uint8_t *pRawPakcet= encodeCCACKPacket(makePacket,packetSize);
		return pRawPakcet;

	};
	uint8_t * sendByForwarder(uint16_t srcId, uint16_t destId)
	{
		cout<<"sending bt forwarder\n";
		bool matching=false;
		uint32_t flowId=getFlowId(srcId,destId);
		cout<<"dest id : flow id" << destId <<flowId <<endl;
		Flow *pFlow=NULL;
		for(int i=0; (unsigned int)i< flows.size() ; i++)
		{
			if (flowId == flows.at(i).flowId)
			{   
				
				matching=true;
				pFlow=flows.at(i).pFlow;
				break;
			}
		}
		if ( matching == false)
		{
			cout <<"ERROR: flowID must match " << endl;
			return NULL;
		}

		// Now we have the flow ready.

		uint8_t *pRawPakcet=NULL;

			pRawPakcet=modifyCCACKpakcet(pFlow);
			//TODO: call send subroutine
		
		cout<<"sne by forwarder\n";
		return pRawPakcet; // TAMAL: TODO: Does not make any sense now.. it can not return only one packet.. Just for tesing
							// remove it... the return type should be "void " for this case
							//TODO: free memory.. call free.. 

	};
	

//TODO: Fixed These 2 functions.
        int updateGlobalForwarders( Flow *pFlow, int fwder, int credit)
	{
		
		pFlow->addGlobalForwarder(fwder,credit);
		//pFlow->addGlobalForwarder(4,8);

		return 1;//TODO: probably size of the list
	}

	int updateLocalForwarders( Flow *pFlow, int rate , vector<Forwarder_s> fwder)
	{
		pFlow->clearLocalForwarder();
		
		pFlow->addLocalForwarder(fwder);



		return 1;//TODO: probably size of the list
	}

};

#ifndef NS3_HACK

void main()
{
	NC_node src(0),recv1(1),recv2(2),dest(3); // Created 4 nodes where 2 are sender and receiver and recv1 and recv2 are intermediate nodes.
	int rate=1; //TODO: finalize;

	src.newPacketBySource(3);

	uint8_t *pLastPacket=NULL;
	Flow *pFlow=src.getFlow(3);
	src.updateGlobalForwarders(pFlow);
	src.updateLocalForwarders(pFlow, rate);
	
	pLastPacket= src.sendBySource(pFlow);
	recv1.checkReceivedCCACK(pLastPacket,64);
	recv1.checkReceivedCCACK(pLastPacket,64);
	dest.checkReceivedCCACK(pLastPacket,64);

	//pLastPacket=recv1.sendB(3);

	//pLastPacket= src.sendBySourceTest(3);
	//recv1.checkReceived(pLastPacket,100);

	//pLastPacket= recv1.sendBySourceTest(3);
	//dest.checkReceived(pLastPacket,100);


	getchar();
	getchar();
	
	
}

#endif
