#include "ergm_changestat_operator.h"
#include "changestats_operator.h"

/* passthrough(formula) */

I_CHANGESTAT_FN(i_passthrough_term){
  double *inputs = INPUT_PARAM;
  // No need to allocate it: we are only storing a pointer to a model.

  STORAGE = unpack_Model_as_double(&inputs);
 
  InitStats(nwp, STORAGE);
}

D_CHANGESTAT_FN(d_passthrough_term){
  GET_STORAGE(Model, m);

  ChangeStats(ntoggles, tails, heads, nwp, m);
  
  memcpy(CHANGE_STAT, m->workspace, N_CHANGE_STATS*sizeof(double));
}

U_CHANGESTAT_FN(u_passthrough_term){
  GET_STORAGE(Model, m);

  UPDATE_STORAGE(tail, head, nwp, m, NULL, edgeflag);
}

F_CHANGESTAT_FN(f_passthrough_term){
  GET_STORAGE(Model, m);

  ModelDestroy(nwp, m);

  STORAGE = NULL;
}

/* .submodel(formula) */

I_CHANGESTAT_FN(i__submodel_term){
  double *inputs = INPUT_PARAM+1;
  // No need to allocate it: we are only storing a pointer to a model.

  AUX_STORAGE = unpack_Model_as_double(&inputs);

  InitStats(nwp, AUX_STORAGE);
}

U_CHANGESTAT_FN(u__submodel_term){
  GET_AUX_STORAGE(Model, m);

  UPDATE_STORAGE(tail, head, nwp, m, NULL, edgeflag);
}

F_CHANGESTAT_FN(f__submodel_term){
  GET_AUX_STORAGE(Model, m);

  ModelDestroy(nwp, m);

  AUX_STORAGE=NULL;
}

/* submodel.test(formula) */

D_CHANGESTAT_FN(d_submodel_test_term){
  GET_AUX_STORAGE(Model, m);

  double *tmp = m->workspace;
  m->workspace = CHANGE_STAT;
  ChangeStats(ntoggles, tails, heads, nwp, m);
  m->workspace = tmp;
}

/* .summary(formula) */


I_CHANGESTAT_FN(i__summary_term){
  double *inputs = INPUT_PARAM+1;
  GET_STORAGE(Model, m); // No need to allocate, since we just need a pointer.

  // Unpack the submodel.
  STORAGE = m = unpack_Model_as_double(&inputs);
  // Initialize empty network.
  Network *tmpnwp = NetworkInitialize(NULL, NULL, 0, N_NODES, DIRECTED, BIPARTITE, 0, 0, NULL);

  // Initialize storage for submodel terms 
  InitStats(tmpnwp, m);

  ALLOC_AUX_STORAGE(m->n_stats, double, stats);
  memcpy(stats, inputs, m->n_stats*sizeof(double));

  // Evaluate the initial summary statistics (the slow way).
  for(Vertex tail=1; tail <= N_TAILS; tail++){
    Vertex head;
    Edge e;
    STEP_THROUGH_OUTEDGES(tail, e, head) {
      ChangeStats(1, &tail, &head, tmpnwp, m);
      for(unsigned int k=0; k<m->n_stats; k++)
	stats[k] += m->workspace[k];
      UPDATE_STORAGE(tail, head, tmpnwp, m, NULL, 1);
      ToggleEdge(tail, head, tmpnwp);
    }
  }
  // Note that nw is now identitical to nwp.
  NetworkDestroy(tmpnwp);
}

U_CHANGESTAT_FN(u__summary_term){
  GET_STORAGE(Model, m);
  GET_AUX_STORAGE(double, stats);

  ChangeStats(1, &tail, &head, nwp, m);
  for(unsigned int k=0; k<m->n_stats; k++)
    stats[k] += m->workspace[k];

  UPDATE_STORAGE(tail, head, nwp, m, NULL, edgeflag);
}

F_CHANGESTAT_FN(f__summary_term){
  GET_STORAGE(Model, m);

  ModelDestroy(nwp, m);

  STORAGE=NULL;
}

/* summary.test(formula) */

C_CHANGESTAT_FN(c_summary_test_term){
  GET_AUX_STORAGE(double, stats);
  CHANGE_STAT[0] = 0;
  /* Rprintf("Test .summary auxiliary: ["); */
  for(unsigned int i=0; i<INPUT_PARAM[1]; i++){
    Rprintf(" %f", stats[i]);
  }
  Rprintf(" \n");
  /* Rprintf(" ]\n"); */
}

F_CHANGESTAT_FN(f_summary_test_term){
  GET_AUX_STORAGE(double, stats);
  /* Rprintf("Test .summary auxiliary (last): ["); */
  for(unsigned int i=0; i<INPUT_PARAM[1]; i++) Rprintf(" %f", stats[i]);
  Rprintf(" \n");
  /* Rprintf(" ]\n"); */
}

/* filter_term_form 

   A term to wrap abitrary binary ergm terms by constructing a binary
   network that mirrors the LHS network in that it has an edge iff the term
   in the second formula contributes +1 due to that dyad.

*/

I_CHANGESTAT_FN(i_filter_term_form){
  double *inputs = INPUT_PARAM;
  GET_AUX_STORAGE(StoreNetAndModel, storage); inputs++;
  Network *bnwp = storage->nwp;

  GET_STORAGE(Model, m); // Only need the pointer, no allocation needed.

  STORAGE = m = unpack_Model_as_double(&inputs);

  InitStats(bnwp, m);
}

C_CHANGESTAT_FN(c_filter_term_form){
  GET_AUX_STORAGE(StoreNetAndModel, storage);
  Network *bnwp = storage->nwp;
  GET_STORAGE(Model, m);

  ChangeStats(1, &tail, &head, nwp, storage->m);
  
  if(*(storage->m->workspace)!=0){ // If the binary view changes...
    ChangeStats(1, &tail, &head, bnwp, m);
    memcpy(CHANGE_STAT, m->workspace, N_CHANGE_STATS*sizeof(double));
  } // Otherwise, leave the change stats at 0.
}

U_CHANGESTAT_FN(u_filter_term_form){
  GET_AUX_STORAGE(StoreNetAndModel, storage);
  Network *bnwp = storage->nwp;
  GET_STORAGE(Model, m);

  ChangeStats(1, &tail, &head, nwp, storage->m);
  
  if(*(storage->m->workspace)!=0){ // If the binary view changes...
    UPDATE_STORAGE(tail, head, bnwp, m, NULL, edgeflag);
  }
}

F_CHANGESTAT_FN(f_filter_term_form){
  GET_AUX_STORAGE(StoreNetAndModel, storage);
  Network *bnwp = storage->nwp;
  GET_STORAGE(Model, m);

  ModelDestroy(bnwp, m);
  STORAGE = NULL;
}

/* _filter_formula_net 

   Maintain a binary network that has an edge wherever the
   contribution of the a given term (edges, nodematch, etc.) whose
   dyadwise value is either 0 or 1 is 1.

*/


I_CHANGESTAT_FN(i__filter_formula_net){
  double *inputs = INPUT_PARAM;
  ALLOC_AUX_STORAGE(1, StoreNetAndModel, storage); inputs++;
  Model *m = storage->m = unpack_Model_as_double(&inputs);
  Network *bnwp = storage->nwp = NetworkInitialize(NULL, NULL, 0, N_NODES, DIRECTED, BIPARTITE, FALSE, 0, NULL);
  InitStats(nwp, m);

  EXEC_THROUGH_NET_EDGES_PRE(t, h, e, {
	ChangeStats(1, &t, &h, nwp, m);
	// I.e., if toggling the dyad changes the statistic, add
	// edge to the filter network.
	if(*(m->workspace)!=0) 
	  AddEdgeToTrees(t, h, bnwp);
    });
}

U_CHANGESTAT_FN(u__filter_formula_net){
  GET_AUX_STORAGE(StoreNetAndModel, storage);
  Model *m = storage->m;
  Network *bnwp = storage->nwp;

  ChangeStats(1, &tail, &head, nwp, m);
  if(*(m->workspace)!=0){
    if(IS_OUTEDGE(tail, head)) DeleteEdgeFromTrees(tail,head,bnwp);
    else AddEdgeToTrees(tail,head,bnwp);
  }

  UPDATE_STORAGE(tail, head, nwp, m, NULL, edgeflag);
}

F_CHANGESTAT_FN(f__filter_formula_net){
  GET_AUX_STORAGE(StoreNetAndModel, storage);
  Model *m = storage->m;
  Network *bnwp = storage->nwp;
  ModelDestroy(nwp, m);
  NetworkDestroy(bnwp);
  // DestroyStats() will deallocate the rest.
}

/* undir */
I_CHANGESTAT_FN(i_undir){
  double *inputs = INPUT_ATTRIB+1;
    
  GET_AUX_STORAGE(Network, unwp);
  Model *m = STORAGE = unpack_Model_as_double(&inputs);
  InitStats(unwp, m);
}

C_CHANGESTAT_FN(c_undir){
  GET_STORAGE(Model, m);
  GET_AUX_STORAGE(Network, unwp);
  unsigned int rule = *INPUT_ATTRIB;

  unsigned int totoggle;
  switch(rule){
  case 1: // weak
    totoggle = !IS_OUTEDGE(head,tail);
    break;
  case 2: // strong
    totoggle = IS_OUTEDGE(head,tail);
    break;
  case 3: // upper
    totoggle = tail<=head;
    break;
  case 4: // lower
    totoggle = tail>=head;
    break;
  default: // never reached, but avoids a warning
    totoggle = FALSE;
  }

  if(totoggle){
    if(tail <= head) ChangeStats(1, &tail, &head, unwp, m);
    else ChangeStats(1, &head, &tail, unwp, m);

    memcpy(CHANGE_STAT, m->workspace, N_CHANGE_STATS*sizeof(double));
  }
}

U_CHANGESTAT_FN(u_undir){
  GET_STORAGE(Model, m);
  GET_AUX_STORAGE(Network, unwp);
  unsigned int rule = *INPUT_ATTRIB;

  unsigned int totoggle;
  switch(rule){
  case 1: // weak
    totoggle = !IS_OUTEDGE(head,tail);
    break;
  case 2: // strong
    totoggle = IS_OUTEDGE(head,tail);
    break;
  case 3: // upper
    totoggle = tail<=head;
    break;
  case 4: // lower
    totoggle = tail>=head;
    break;
  default: // never reached, but avoids a warning
    totoggle = FALSE;
  }

  if(totoggle) UPDATE_STORAGE(MIN(tail,head), MAX(tail,head), unwp, m, NULL, IS_UNDIRECTED_EDGE(tail,head,unwp));
}

F_CHANGESTAT_FN(f_undir){
  GET_STORAGE(Model, m);
  GET_AUX_STORAGE(Network, unwp);
  ModelDestroy(unwp, m);
  STORAGE = NULL;
}


// Sum: Take a weighted sum of the models' statistics.

I_CHANGESTAT_FN(i_Sum){
  /*
    inputs expected:
    1: number of models (nms)
    1: total length of all weight matrices (tml)
    tml: a list of mapping matrices in row-major order
    nms*?: submodel specifications for nms submodels
  */
  
  double *inputs = INPUT_PARAM; 
  unsigned int nms = *(inputs++);
  unsigned int tml = *(inputs++);
  inputs+=tml; // Jump to start of model specifications.
  
  ALLOC_STORAGE(nms, Model*, ms);

  for(unsigned int i=0; i<nms; i++){
    ms[i] = unpack_Model_as_double(&inputs);
    InitStats(nwp, ms[i]);
  }
}

C_CHANGESTAT_FN(c_Sum){
  double *inputs = INPUT_PARAM; 
  GET_STORAGE(Model*, ms);
  unsigned int nms = *(inputs++);
  inputs++; //  Skip total length of weight matrices.
  double *wts = inputs;

  for(unsigned int i=0; i<nms; i++){
    Model *m = ms[i];
    ChangeStats(1, &tail, &head, nwp, m);
    for(unsigned int j=0; j<m->n_stats; j++)
      for(unsigned int k=0; k<N_CHANGE_STATS; k++)
	CHANGE_STAT[k] += m->workspace[j]* *(wts++);
  }
}

U_CHANGESTAT_FN(u_Sum){
  double *inputs = INPUT_PARAM; 
  GET_STORAGE(Model*, ms);
  unsigned int nms = *(inputs++);

  for(unsigned int i=0; i<nms; i++){
    Model *m = ms[i];
    UPDATE_STORAGE(tail, head, nwp, m, NULL, edgeflag);
  }
}

F_CHANGESTAT_FN(f_Sum){
  double *inputs = INPUT_PARAM; 
  GET_STORAGE(Model*, ms);
  unsigned int nms = *(inputs++);

  for(unsigned int i=0; i<nms; i++){
    ModelDestroy(nwp, ms[i]);
  }
}
