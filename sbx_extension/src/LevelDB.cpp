#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/write_batch.h"
#include "LevelDB.hpp"
#include <godot_cpp/classes/project_settings.hpp>
#include "pocketpy.h"

namespace pkpy {

struct LevelDB {
    leveldb::DB* db;
    leveldb::Iterator* iter;    // This is shared. It ensures iterator was properly deleted when DB is closed.
    const leveldb::FilterPolicy* filter_policy;

    void reset_iter(leveldb::Iterator* new_iter) {
        if(iter != NULL) delete iter;
        iter = new_iter;
    }

    void close() {
        reset_iter(NULL);

        if(db != NULL) {
            delete db;
            db = NULL;
        }
        if(filter_policy != NULL) {
            delete filter_policy;
            filter_policy = NULL;
        }
    }
};

void setup_leveldb_module() {
    py_GlobalRef mod = py_newmodule("leveldb");

    py_Type tp_DB = py_newtype("DB", tp_object, mod, [](void *ud) {
        LevelDB* p_db = (LevelDB*)ud;
        p_db->close();
    });
    py_tpsetfinal(tp_DB);

    py_Type tp_DBIter = py_newtype("_DBIter", tp_object, mod, NULL);
    py_tpsetfinal(tp_DBIter);

    py_bind(py_tpobject(tp_DB), "__new__(cls, path: str, create_if_missing=False, error_if_exists=False, paranoid_checks=False, bloom_filter_policy_bits=0)", [](int argc, py_Ref argv) -> bool {
        py_Type cls = py_totype(py_arg(0));
        PY_CHECK_ARG_TYPE(1, tp_str);
        PY_CHECK_ARG_TYPE(2, tp_bool);
        PY_CHECK_ARG_TYPE(3, tp_bool);
        PY_CHECK_ARG_TYPE(4, tp_bool);
        PY_CHECK_ARG_TYPE(5, tp_int);
        const char* path = py_tostr(py_arg(1));
        leveldb::DB* db;
        leveldb::Options options;
        options.create_if_missing = py_tobool(py_arg(2));
        options.error_if_exists = py_tobool(py_arg(3));
        options.paranoid_checks = py_tobool(py_arg(4));
        py_i64 bloom_filter_policy_bits = py_toint(py_arg(5));
        if (bloom_filter_policy_bits > 0) {
            options.filter_policy = leveldb::NewBloomFilterPolicy(bloom_filter_policy_bits);
        } else {
            options.filter_policy = NULL;
        }

        godot::String global_path = godot::ProjectSettings::get_singleton()->globalize_path(path);
        godot::print_line("LevelDB opening path: " + global_path);

        leveldb::Status status = leveldb::DB::Open(options, global_path.utf8().get_data(), &db);
        if (!status.ok()) {
            delete options.filter_policy;
            return RuntimeError("LevelDB failed to open: %s", status.ToString().c_str());
        }
        void* ud = py_newobject(py_retval(), cls, 0, sizeof(LevelDB));
        LevelDB* p_db = (LevelDB*)ud;
        p_db->db = db;
        p_db->iter = NULL;
        p_db->filter_policy = options.filter_policy;
        return true;
    });

    py_bind(py_tpobject(tp_DB), "get(self, key: str, verify_checksums=False) -> bytes | None", [](int argc, py_Ref argv) -> bool {
        LevelDB* self = (LevelDB*)py_touserdata(py_arg(0));
        if(self->db == NULL) return RuntimeError("LevelDB is closed");
        PY_CHECK_ARG_TYPE(1, tp_str);
        PY_CHECK_ARG_TYPE(2, tp_bool);
        const char* key = py_tostr(py_arg(1));
        std::string value;
        leveldb::ReadOptions options;
        options.verify_checksums = py_tobool(py_arg(2));
        leveldb::Status status = self->db->Get(options, key, &value);
        if (!status.ok()) {
            if (status.IsNotFound()) {
                py_newnone(py_retval());
                return true;
            }
            return RuntimeError("LevelDB failed to get: %s", status.ToString().c_str());
        }
        void* p = py_newbytes(py_retval(), value.size());
        std::memcpy(p, value.data(), value.size());
        return true;
    });

    py_bind(py_tpobject(tp_DB), "put(self, key: str, value: bytes, sync=False) -> None", [](int argc, py_Ref argv) -> bool {
        LevelDB* self = (LevelDB*)py_touserdata(py_arg(0));
        if(self->db == NULL) return RuntimeError("LevelDB is closed");
        PY_CHECK_ARG_TYPE(1, tp_str);
        PY_CHECK_ARG_TYPE(2, tp_bytes);
        PY_CHECK_ARG_TYPE(3, tp_bool);
        const char* key = py_tostr(py_arg(1));
        int value_size;
        unsigned char* value = py_tobytes(py_arg(2), &value_size);
        leveldb::Slice value_slice((const char*)value, value_size);
        leveldb::WriteOptions options;
        options.sync = py_tobool(py_arg(3));
        leveldb::Status status = self->db->Put(options, key, value_slice);
        if (!status.ok()) {
            return RuntimeError("LevelDB failed to put: %s", status.ToString().c_str());
        }
        py_newnone(py_retval());
        return true;
    });

    py_bind(py_tpobject(tp_DB), "delete(self, key: str, sync=False)", [](int argc, py_Ref argv) -> bool {
        LevelDB* self = (LevelDB*)py_touserdata(py_arg(0));
        if(self->db == NULL) return RuntimeError("LevelDB is closed");
        PY_CHECK_ARG_TYPE(1, tp_str);
        PY_CHECK_ARG_TYPE(2, tp_bool);
        const char* key = py_tostr(py_arg(1));
        leveldb::WriteOptions options;
        options.sync = py_tobool(py_arg(2));
        leveldb::Status status = self->db->Delete(options, key);
        if (!status.ok()) {
            return RuntimeError("LevelDB failed to delete: %s", status.ToString().c_str());
        }
        py_newnone(py_retval());
        return true;
    });

    py_bindmethod(tp_DB, "close", [](int argc, py_Ref argv) -> bool {
        LevelDB* self = (LevelDB*)py_touserdata(py_arg(0));
        self->close();
        py_newnone(py_retval());
        return true;
    });

    py_bind(py_tpobject(tp_DB), "write(self, ops: dict[str, bytes | None], sync=False)", [](int argc, py_Ref argv) -> bool {
        LevelDB* self = (LevelDB*)py_touserdata(py_arg(0));
        if(self->db == NULL) return RuntimeError("LevelDB is closed");
        PY_CHECK_ARG_TYPE(1, tp_dict);
        PY_CHECK_ARG_TYPE(2, tp_bool);

        py_StackRef ops = py_arg(1);

        leveldb::WriteBatch batch;
        leveldb::WriteOptions options;
        options.sync = py_tobool(py_arg(2));

        py_dict_apply(ops, [](py_Ref key, py_Ref value, void* arg) {
            leveldb::WriteBatch* batch = (leveldb::WriteBatch*)arg;
            if(!py_checktype(key, tp_str)) return false;
            const char* key_str = py_tostr(key);
            if(py_isnone(value)) {
                batch->Delete(key_str);
            } else {
                if(!py_checktype(value, tp_bytes)) return false;
                int value_size;
                unsigned char* value_bytes = py_tobytes(value, &value_size);
                leveldb::Slice value_slice((const char*)value_bytes, value_size);
                batch->Put(key_str, value_slice);
            }
            return true;
        }, &batch);

        leveldb::Status status = self->db->Write(options, &batch);
        if (!status.ok()) {
            return RuntimeError("LevelDB failed to write batch: %s", status.ToString().c_str());
        }
        py_newnone(py_retval());
        return true;
    });

    py_bind(py_tpobject(tp_DB), "iter(self, start: str | None = None, end: str | None = None, verify_checksums=False)", [](int argc, py_Ref argv) -> bool {
        LevelDB* self = (LevelDB*)py_touserdata(py_arg(0));
        if(self->db == NULL) return RuntimeError("LevelDB is closed");

        const char* start = NULL;
        const char* end = NULL;
        if (!py_isnone(py_arg(1))) {
            PY_CHECK_ARG_TYPE(1, tp_str);
            start = py_tostr(py_arg(1));
        }
        if (!py_isnone(py_arg(2))) {
            PY_CHECK_ARG_TYPE(2, tp_str);
            end = py_tostr(py_arg(2));
        }
        PY_CHECK_ARG_TYPE(3, tp_bool);
        leveldb::ReadOptions options;
        options.verify_checksums = py_tobool(py_arg(3));
        options.fill_cache = false;

        self->reset_iter(self->db->NewIterator(options));

        if (start) {
            self->iter->Seek(start);
        } else {
            self->iter->SeekToFirst();
        }
        py_Type cls = py_gettype("leveldb", py_name("_DBIter"));
        py_newobject(py_retval(), cls, 2, 0);
        py_setslot(py_retval(), 0, argv);
        if(end == NULL) {
            py_newnone(py_getslot(py_retval(), 1));
        } else {
            py_assign(py_getslot(py_retval(), 1), py_arg(2));
        }
        return true;
    });

    py_bindmethod(tp_DBIter, "__iter__", [](int argc, py_Ref argv) -> bool {
        PY_CHECK_ARGC(1);
        py_assign(py_retval(), argv);
        return true;
    });

    py_bindmethod(tp_DBIter, "__next__", [](int argc, py_Ref argv) -> bool {
        PY_CHECK_ARGC(1);
        py_Ref leveldb = py_getslot(py_arg(0), 0);
        py_Ref end_ref = py_getslot(py_arg(0), 1);

        LevelDB* self = (LevelDB*)py_touserdata(leveldb);
        if(self->db == NULL) return RuntimeError("LevelDB is closed");

        if(self->iter == NULL) return StopIteration();

        const char* end = NULL;
        if (!py_isnone(end_ref)) end = py_tostr(end_ref);

        if (!self->iter->Valid()) {
            if(self->iter->status().ok()){
                return StopIteration();
            } else {
                return RuntimeError("LevelDB iterator error: %s", self->iter->status().ToString().c_str());
            }
        }
        if (end && self->iter->key().compare(end) >= 0) {
            return StopIteration();
        }
        
        leveldb::Slice key = self->iter->key();
        leveldb::Slice value = self->iter->value();
        py_Ref tuple = py_newtuple(py_retval(), 2);
        py_newstrv(py_tuple_getitem(tuple, 0), c11_sv{key.data(), (int)key.size()});
        unsigned char* p = py_newbytes(py_tuple_getitem(tuple, 1), value.size());
        std::memcpy(p, value.data(), value.size());
        self->iter->Next();
        return true;
    });
}

}
