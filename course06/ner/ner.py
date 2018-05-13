import multiprocessing
import os
import time


models = ["ivlblskip", "ivlblcbow"]
#models = ["rand"]

def func(msg, vec_dir, ret_dir):
    vec_file = "%s/%s" % (vec_dir, msg)
    out_file = "%s/%s" % (ret_dir, msg)
    arg = "java -jar ner.jar %s %s > %s" % (vec_file.replace("/","_").replace(":","_"), vec_file, out_file)
    if not os.path.exists(out_file):
        print arg
        os.system(arg)


if __name__ == "__main__":
    pool = multiprocessing.Pool(processes=3)

    for model in models:
        vec_dir = "vec_%s" % model
        ret_dir = "ret_ner_%s" % model

        if not os.path.exists(ret_dir):
            os.makedirs(ret_dir)
        
        for lists in os.listdir(vec_dir):
            if not os.path.exists(os.path.join(ret_dir, lists)):

                x = lists.replace('.txt','').replace('.bz2','').split('_')
                #if not "all_1b" in lists:
                #    continue
                iter = int(x[-1])
                if not (iter == 1 or iter == 3 or iter == 5 or iter == 20 or iter == 10 or iter == 33 or iter == 100 or iter == 1000 or iter == 10000):
                    continue
                print lists
                pool.apply_async(func, (lists, vec_dir, ret_dir, ))
    pool.close()
    pool.join()
    print "Sub-process(es) done."
    
    
