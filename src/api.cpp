#include <dplyr.h>

namespace dplyr{ 

    void CallProxy::set_call( SEXP call_ ){
        proxies.clear() ;
        call = call_ ;
        if( TYPEOF(call) == LANGSXP ) traverse_call(call) ;
    }
    
    SEXP CallProxy::eval(){
        if( TYPEOF(call) == LANGSXP ){
                     
            if( can_simplify(call) ){
                SlicingIndex indices(0,subsets.nrows()) ;
                while(simplified(indices)) ;
                set_call(call) ;
            }
            
            int n = proxies.size() ;
            for( int i=0; i<n; i++){
                proxies[i].set( subsets[proxies[i].symbol] ) ;
            }
            return call.eval(env) ;
        } else if( TYPEOF(call) == SYMSXP) {
            // SYMSXP
            if( subsets.count(call) ) return subsets.get_variable(call) ;
            return call.eval(env) ;
        }
        return call ;
    }    
    
    bool CallProxy::simplified(const SlicingIndex& indices){
        // initial
        if( TYPEOF(call) == LANGSXP ){
            boost::scoped_ptr<Result> res( get_handler(call, subsets, env) );
            
            if( res ){
                // replace the call by the result of process
                call = res->process(indices) ;
    
                // no need to go any further, we simplified the top level
                return true ;
            }
            
            return replace( CDR(call), indices ) ;
            
        }
        return false ;
    }
        
    bool CallProxy::replace( SEXP p, const SlicingIndex& indices ){
        
        SEXP obj = CAR(p) ;
        
        if( TYPEOF(obj) == LANGSXP ){
            boost::scoped_ptr<Result> res( get_handler(obj, subsets, env) );
            if(res){
                SETCAR(p, res->process(indices) ) ;
                return true ;
            }
            
            if( replace( CDR(obj), indices ) ) return true ;   
        }     
        
        if( TYPEOF(p) == LISTSXP ){
            return replace( CDR(p), indices ) ;    
        }
          
        return false ;
    }
          
    void CallProxy::traverse_call( SEXP obj ){
        
        if( TYPEOF(obj) == LANGSXP && CAR(obj) == Rf_install("local") ) return ;
        if( ! Rf_isNull(obj) ){
            SEXP head = CAR(obj) ;
            switch( TYPEOF( head ) ){
            case LANGSXP:
                if( CAR(head) == Rf_install("order_by") ) break ;
                if( CAR(head) == Rf_install("function") ) break ;
                if( CAR(head) == Rf_install("local") ) return ;
                if( CAR(head) == Rf_install("<-") ){
                    stop( "assignments are forbidden" ) ;
                }
                if( Rf_length(head) == 3 ){
                    SEXP symb = CAR(head) ;
                    if( symb == R_DollarSymbol || symb == Rf_install("@") || symb == Rf_install("::") || symb == Rf_install(":::") ){
                        
                        // Rprintf( "CADR(obj) = " ) ;
                        // Rf_PrintValue( CADR(obj) ) ;
                        
                        // for things like : foo( bar = bling )$bla
                        // so that `foo( bar = bling )` gets processed
                        if( TYPEOF(CADR(head)) == LANGSXP ){
                            traverse_call( CDR(head) ) ;    
                        }
                        
                        // deal with foo$bar( bla = boom )
                        if( TYPEOF(CADDR(head)) == LANGSXP ){
                            traverse_call( CDDR(head) ) ;
                        }
                        
                        break ;
                    } else {
                      traverse_call( CDR(head) ) ;
                    }
                } else {
                    traverse_call( CDR(head) ) ;
                }
    
                break ;
            case LISTSXP:
                traverse_call( head ) ;
                traverse_call( CDR(head) ) ;
                break ;
            case SYMSXP:
                if( TYPEOF(obj) != LANGSXP ){
                    if( ! subsets.count(head) ){
                        if( head == R_MissingArg ) break ;
                        if( head == Rf_install(".") ) break ;
    
                        // in the Environment -> resolve
                        try{
                            Shield<SEXP> x( env.find( CHAR(PRINTNAME(head)) ) ) ;
                            SETCAR( obj, x );
                        } catch( ...){
                            // what happens when not found in environment
                        }
    
                    } else {
                        // in the data frame
                        proxies.push_back( CallElementProxy( head, obj ) );
                    }
                    break ;
                }
            }
            traverse_call( CDR(obj) ) ;
        }
    }

}
 